# Complete Software & Firmware Architecture

This document presents the detailed architectural design of the ESP32 deployment firmware.

---

## 1. Modular Layer Diagram

```
+-------------------------------------------------------------------+
|                        APPLICATION LAYER                          |
|   src/main.cpp (State Machine Orchestrator & Serial CLI Console) |
+-------------------------------+-----------------------------------+
                                |
        +-----------------------+-----------------------+
        |                                               |
+-------v-----------------------+       +---------------+-------+
|    COMMUNICATION & LOGGING    |       |    SIGNAL PROCESSING  |
|  - WiFi Manager (HTTP Server) |       |  - Sensor Fusion      |
|  - Storage Logger (LittleFS)  |       |  - Attitude & Heading |
|  - Diagnostics Self-Test      |       |  - PI Signal Engine   |
+-------+-----------------------+       +---------------+-------+
        |                                               |
        +-----------------------+-----------------------+
                                |
+-------------------------------+-----------------------------------+
|                        HARDWARE DRIVERS                           |
|  - MPU6050 Driver (I2C)     - DS3231 Driver (I2C)                 |
|  - RM3100 Driver (SPI)      - MAX31865 Driver (SPI)               |
+-------------------------------+-----------------------------------+
                                |
+-------------------------------+-----------------------------------+
|                  HARDWARE ABSTRACTION LAYER (HAL)                 |
|  - ESP32 Wire (I2C)         - ESP32 SPI (SPISettings per dev)    |
|  - ESP32 ADC1 (GPIO32)      - ESP32 GPIO (GPIO33 Gate Pulse)      |
+-------------------------------------------------------------------+
```

---

## 2. Directory Structure

- `platformio.ini` : Project build configuration file.
- `src/config.h` : Central configuration macros, GPIO pin mappings, and parameter constants.
- `src/sensors/` : Sensor drivers (MPU6050, DS3231, MAX31865, RM3100).
- `src/pi/` : Pulse Induction coil controller and anomaly detector.
- `src/fusion/` : Attitude complementary filter, tilt-compensated heading, drift sector estimator.
- `src/logging/` : Data logger supporting CSV and LittleFS storage.
- `src/communication/` : Post-recovery Wi-Fi Access Point and HTTP server.
- `src/diagnostics/` : System self-test table and hardware health diagnostics.
- `src/main.cpp` : Entry point, non-blocking time-multiplexed loop state machine, CLI command parser.
