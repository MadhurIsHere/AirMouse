#include <Arduino.h>
#include <Wire.h>
#include <BleMouse.h>

BleMouse bleMouse("ESP32 Air Mouse");

constexpr int SDA_PIN = 8;
constexpr int SCL_PIN = 9;
constexpr int LEFT_BUTTON = 4;
constexpr int RIGHT_BUTTON = 5;

const uint8_t MPU_ADDR = 0x68;

#define REG_WHO_AM_I     0x75
#define REG_PWR_MGMT_1   0x6B
#define REG_PWR_MGMT_2   0x6C
#define REG_CONFIG       0x1A
#define REG_GYRO_CONFIG  0x1B
#define REG_ACCEL_CONFIG 0x1C
#define REG_ACCEL_XOUT_H 0x3B

// Gyro FS = ±500 dps -> 65.5 LSB/(dps)
const float gyro_scale = 65.5f;

// ---- Tuning ----
float sensitivity = 30.0f;   // pixels per degree of rotation
float deadzone    = 1.2f;   // deg/s (1.2 dps completely eliminates resting noise/drift)
float alpha       = 0.35f;  // low-pass filter: lower = smoother, less jitter
const unsigned long RECAL_HOLD_MS = 2000;

bool sensorOK = false;

// Gyro bias
float gx_bias = 0, gy_bias = 0, gz_bias = 0;

// Filter state
float fx = 0, fy = 0, fz = 0;

// Sub-pixel accumulators
float accX = 0, accY = 0;

bool previousLeft = HIGH;
bool previousRight = HIGH;
bool wasConnected = false;

unsigned long lastUs = 0;
unsigned long bothHeldSince = 0;

void writeReg(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}

uint8_t readReg(uint8_t reg) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_ADDR, (uint8_t)1);
  if (Wire.available()) return Wire.read();
  return 0xFF;
}

// Byte-order safe 16-bit big-endian reader
static inline int16_t read16BE() {
  uint8_t hi = Wire.read();
  uint8_t lo = Wire.read();
  return (int16_t)((hi << 8) | lo);
}

// Read accel + temp + gyro in one burst (14 bytes)
bool readAll(int16_t &ax, int16_t &ay, int16_t &az,
             int16_t &gx, int16_t &gy, int16_t &gz) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(REG_ACCEL_XOUT_H);
  if (Wire.endTransmission(false) != 0) return false;

  if (Wire.requestFrom(MPU_ADDR, (uint8_t)14) != 14) return false;

  ax = read16BE();
  ay = read16BE();
  az = read16BE();
  Wire.read(); Wire.read(); // skip temperature
  gx = read16BE();
  gy = read16BE();
  gz = read16BE();
  return true;
}

void setupSensor() {
  writeReg(REG_PWR_MGMT_1, 0x00);
  delay(50);
  writeReg(REG_PWR_MGMT_1, 0x01);
  delay(20);
  writeReg(REG_CONFIG, 0x03);
  writeReg(REG_GYRO_CONFIG, 0x08);   // ±500 dps
  writeReg(REG_ACCEL_CONFIG, 0x10);  // ±8g
  writeReg(REG_PWR_MGMT_2, 0x00);
  delay(20);
}

void calibrateGyro() {
  Serial.println("\n=======================================================");
  Serial.println("[CALIBRATION] Sampling resting gyro bias (2 seconds)...");
  delay(1000);

  const int N = 300;
  long sx = 0, sy = 0, sz = 0;
  int count = 0;
  int16_t ax, ay, az, gx, gy, gz;

  for (int i = 0; i < N; i++) {
    if (readAll(ax, ay, az, gx, gy, gz)) {
      sx += gx;
      sy += gy;
      sz += gz;
      count++;
    }
    delay(5);
  }

  if (count > 0) {
    gx_bias = (float)sx / (float)count / gyro_scale;
    gy_bias = (float)sy / (float)count / gyro_scale;
    gz_bias = (float)sz / (float)count / gyro_scale;
  }

  // Clear filter and accumulator
  fx = fy = fz = 0;
  accX = accY = 0;

  Serial.printf("[CALIBRATION] Done! Offsets subtracted: X=%.2f Y=%.2f Z=%.2f\n", gx_bias, gy_bias, gz_bias);
  Serial.println("=======================================================\n");
}

inline int8_t clampMove(int v) {
  if (v > 127) return 127;
  if (v < -127) return -127;
  return (int8_t)v;
}

static inline float softDeadzone(float v, float d) {
  if (fabsf(v) < d) return 0.0f;
  return v > 0.0f ? (v - d) : (v + d);
}

void setup() {
  Serial.begin(115200);
  delay(1500);

  pinMode(LEFT_BUTTON, INPUT_PULLUP);
  pinMode(RIGHT_BUTTON, INPUT_PULLUP);

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000);
  Wire.setTimeOut(100);

  Serial.println("\n==========================================");
  Serial.println("     ESP32-S3 Air Mouse Initialized       ");
  Serial.println("==========================================");

  uint8_t who = readReg(REG_WHO_AM_I);
  Serial.printf("WHO_AM_I: 0x%02X\n", who);

  // Accept any responding I2C sensor (0x68, 0x70, 0x71, 0x72, 0x73, etc.)
  if (who != 0x00 && who != 0xFF) {
    Serial.printf("Sensor accepted (WHO_AM_I: 0x%02X). Initializing...\n", who);
    setupSensor();
    calibrateGyro();
    sensorOK = true;
  } else {
    Serial.println("No sensor response (0x00 / 0xFF)! Check wiring / AD0->GND / power.");
  }

  bleMouse.begin();
  Serial.println("Pair 'ESP32 Air Mouse' in Bluetooth settings.");
  Serial.println("Tip: Hold BOTH buttons for 2 seconds anytime to re-center/re-calibrate.");

  lastUs = micros();
}

void loop() {
  bool connected = bleMouse.isConnected();

  // Reset button state on BLE disconnect
  if (wasConnected && !connected) {
    previousLeft = HIGH;
    previousRight = HIGH;
  }
  wasConnected = connected;

  // ---- Buttons ----
  bool left  = digitalRead(LEFT_BUTTON);
  bool right = digitalRead(RIGHT_BUTTON);

  if (connected) {
    if (previousLeft == HIGH && left == LOW)    bleMouse.press(MOUSE_LEFT);
    if (previousLeft == LOW  && left == HIGH)   bleMouse.release(MOUSE_LEFT);
    if (previousRight == HIGH && right == LOW)  bleMouse.press(MOUSE_RIGHT);
    if (previousRight == LOW  && right == HIGH) bleMouse.release(MOUSE_RIGHT);
  }
  previousLeft = left;
  previousRight = right;

  // ---- Recalibrate: hold both buttons for 2 s while board is still ----
  if (sensorOK && left == LOW && right == LOW) {
    if (bothHeldSince == 0) bothHeldSince = millis();
    if (millis() - bothHeldSince >= RECAL_HOLD_MS) {
      if (connected) {
        bleMouse.release(MOUSE_LEFT);
        bleMouse.release(MOUSE_RIGHT);
      }
      calibrateGyro();
      bothHeldSince = 0;
      // Wait for buttons to be released to avoid retriggering
      while (digitalRead(LEFT_BUTTON) == LOW || digitalRead(RIGHT_BUTTON) == LOW) {
        delay(10);
      }
      previousLeft = HIGH;
      previousRight = HIGH;
      lastUs = micros();
      return;
    }
  } else {
    bothHeldSince = 0;
  }

  if (!sensorOK) {
    delay(20);
    return;
  }

  // ---- Sensor burst read ----
  int16_t ax, ay, az, gx, gy, gz;
  if (!readAll(ax, ay, az, gx, gy, gz)) {
    delay(10);
    return;
  }

  float gx_dps = ((float)gx / gyro_scale) - gx_bias;
  float gy_dps = ((float)gy / gyro_scale) - gy_bias;
  float gz_dps = ((float)gz / gyro_scale) - gz_bias;

  // ---- Low-pass filter ----
  fx = alpha * gx_dps + (1.0f - alpha) * fx;
  fy = alpha * gy_dps + (1.0f - alpha) * fy;
  fz = alpha * gz_dps + (1.0f - alpha) * fz;

  // ---- Soft deadzone ----
  float x_dps = softDeadzone(fx, deadzone);
  float y_dps = softDeadzone(fy, deadzone);
  float z_dps = softDeadzone(fz, deadzone);

  // ---- Dynamic Auto-Bias Trim (cancels thermal drift when resting) ----
  static unsigned long stillStartTime = 0;
  if (fabsf(gx_dps) < 4.5f && fabsf(gy_dps) < 4.5f && fabsf(gz_dps) < 4.5f) {
    if (stillStartTime == 0) stillStartTime = millis();
    if (millis() - stillStartTime > 400) {
      gx_bias += 0.005f * gx_dps;
      gy_bias += 0.005f * gy_dps;
      gz_bias += 0.005f * gz_dps;
    }
  } else {
    stillStartTime = 0;
  }

  // ---- Time delta ----
  unsigned long nowUs = micros();
  float dt = (float)(nowUs - lastUs) / 1000000.0f;
  lastUs = nowUs;
  if (dt > 0.05f) dt = 0.01f;

  // ---- Mouse movement ----
  if (connected) {
    if (x_dps == 0.0f && z_dps == 0.0f) {
      // Resting: smoothly decay accumulated fractions to 0 so they don't slowly leak into cursor movement
      accX *= 0.8f;
      accY *= 0.8f;
    } else {
      // Yaw (-gz) -> horizontal, Pitch (-gx) -> vertical
      accX += -z_dps * sensitivity * dt;
      accY += -x_dps * sensitivity * dt;

      int moveX = (int)accX;
      int moveY = (int)accY;
      accX -= (float)moveX;
      accY -= (float)moveY;

      if (moveX != 0 || moveY != 0) {
        bleMouse.move(clampMove(moveX), clampMove(moveY));
      }
    }
  }

  // ---- Debug print ----
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint >= 500) {
    lastPrint = millis();
    Serial.printf("BLE:%d sensorOK:%d | X=%6.2f Y=%6.2f Z=%6.2f | dt=%.4f\n",
                  connected, sensorOK, x_dps, y_dps, z_dps, dt);
  }

  delay(10); // ~100 Hz
}