# ESP32-S3 BLE Air Mouse 🖱️✨

An ultra-responsive, drift-free Bluetooth Low Energy (BLE) Air Mouse powered by an **ESP32-S3** and an **MPU-6050 / MPU-6500** 6-axis Inertial Measurement Unit (IMU). 

Point and control your computer's mouse cursor in mid-air with natural wrist movements, featuring physical left and right click buttons.

---

## 🚀 Features

- **True Air Mouse Pointing:** High-speed gyroscope tracking mapped to cursor velocity.
- **Sub-Pixel Accumulation:** Eliminates integer truncation jitter, allowing smooth, pixel-accurate micro-movements.
- **Microsecond Delta-Time (`dt`) Scaling:** Cursor speed adapts dynamically to frame time variations for consistent tracking across all machines.
- **Anti-Drift Engine:**
  - **Motion-Sensing Calibration:** Automatically verifies resting stillness during boot before locking zero-bias.
  - **Dynamic Auto-Bias Trim:** Background thermal compensation cancels drift as the sensor warms up.
  - **Soft Deadzone:** Seamless curve transitions near rest that prevent jumpiness.
- **Fast 400 kHz Burst Reads:** Reads all 14 accelerometer, gyro, and temperature registers in a single atomic I²C transaction.
- **Native Bluetooth HID:** Compatible with Windows, macOS, Linux, Android, and iOS without third-party drivers or dongles.
- **On-the-Fly Recalibration:** Hold both buttons simultaneously for 2 seconds anytime to re-center and calibrate.

---

## 🔌 Hardware Pinout & Wiring

Designed for ESP32-S3 (including N16R8 modules, leaving Octal PSRAM/Flash pins GPIO 26–37 untouched):

### 1. MPU-6050 / MPU-6500 $\rightarrow$ ESP32-S3

| Sensor Pin | ESP32-S3 Pin | Notes |
| :--- | :--- | :--- |
| **VCC** | **5V / VBUS** (or **3V3**) | GY-521 modules with onboard 3.3V LDO work best on 5V/VBUS. |
| **GND** | **GND** | Common ground. |
| **SDA** | **GPIO 8** | I²C Serial Data. |
| **SCL** | **GPIO 9** | I²C Serial Clock. |
| **AD0** | **GND** | Locks I²C address to `0x68` (prevents floating address drops). |
| **INT** | *Not Connected* | Optional interrupt line. |

### 2. Tactile Buttons $\rightarrow$ ESP32-S3

Both buttons use the internal pull-up resistors (`INPUT_PULLUP`):

| Button | ESP32-S3 Pin | Other Leg |
| :--- | :--- | :--- |
| **Left Click** | **GPIO 4** | **GND** |
| **Right Click** | **GPIO 5** | **GND** |

---

## 🛠️ Project Setup & Installation

### Requirements
- [PlatformIO IDE](https://platformio.org/) (VS Code extension or Core CLI)
- ESP32-S3 Development Board

### Dependencies (automatically handled by PlatformIO)
- [`t-vk/ESP32 BLE Mouse`](https://github.com/T-vK/ESP32-BLE-Mouse)
- [`adafruit/Adafruit MPU6050`](https://github.com/adafruit/Adafruit_MPU6050)
- [`adafruit/Adafruit Unified Sensor`](https://github.com/adafruit/Adafruit_Sensor)
- [`adafruit/Adafruit BusIO`](https://github.com/adafruit/Adafruit_BusIO)

### Building & Flashing

1. Clone this repository:
   ```bash
   git clone https://github.com/MadhurIsHere/AirMouse.git
   cd AirMouse
   ```
2. Build the project:
   ```bash
   pio run
   ```
3. Upload to your ESP32-S3:
   ```bash
   pio run -t upload
   ```
4. Open the Serial Monitor (115200 baud):
   ```bash
   pio device monitor -b 115200
   ```

---

## 🎮 How to Use

1. **Boot Calibration:**
   - When powered on, leave the device resting still on your desk for 2 seconds while gyro bias is measured.
2. **Pairing:**
   - Open Bluetooth settings on your PC/Laptop.
   - Scan for new devices and pair with **`ESP32 Air Mouse`**.
3. **Control:**
   - Hold the device and point it toward the screen.
   - Tilt/rotate your wrist to move the cursor.
   - Use the buttons on **GPIO 4** and **GPIO 5** for left and right clicking.
4. **Quick Re-zeroing:**
   - If the cursor drifts after aggressive movement, rest it on the table and **hold both buttons down for 2 seconds** to recalibrate.

---

## ⚙️ Tuning Parameters

You can customize sensitivity and smoothing in `src/main.cpp`:

```cpp
float sensitivity = 30.0f;   // Cursor speed (pixels per degree of angular rotation)
float deadzone    = 1.2f;    // Deadband in deg/s to ignore hand tremors
float alpha       = 0.35f;   // Low-pass filter (lower = smoother, higher = more responsive)
```

---

## 📜 License

MIT License. Free for maker, educational, and commercial projects.
