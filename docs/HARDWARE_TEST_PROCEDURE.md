# Hardware Bring-Up & Bench Test Procedure

This document provides step-by-step physical bench testing procedures for hardware engineers bringing up the sensor electronics.

---

## 1. Safety & Power Pre-Checks
1. **Visual Continuity Check:** Inspect all soldering on EXT1 and EXT2 headers. Ensure no shorts exist between 5V/3.3V power pins and GND.
2. **Voltage Level Verification:** Supply 5V to the main power terminal. Measure ESP32 3.3V rail with a multimeter to ensure $3.30\,\text{V} \pm 0.05\,\text{V}$.
3. **Ground Continuity:** Confirm zero resistance between sensor grounds, ESP32 GND pins, and MOSFET source.

---

## 2. Step-by-Step Bench Bring-Up Sequence

### Step 1: Serial CLI Verification
- Connect ESP32 USB cable. Open serial terminal at 115200 baud.
- Verify boot log displays self-test table:
  ```
  ========================================
  SYSTEM SELF-TEST DIAGNOSTIC TABLE
  ========================================
  MPU6050        [OK]   Addr: 0x69
  DS3231         [OK]   Addr: 0x68
  MAX31865       [OK]   CS: GPIO5, Rref: 430.0
  RM3100         [OK]   CS: GPIO4
  PI DRIVER      [READY] Gate: GPIO33, ADC: GPIO32
  LOGGER         [READY] LittleFS Mounted
  WIFI RECOVERY  [READY] AP Mode Standing By
  ========================================
  ```

### Step 2: I2C Bus Diagnostics
- Type `i2cscan`. Confirm devices detected at `0x68` (DS3231) and `0x69` (MPU6050).
- If MPU6050 appears at `0x68`, `AD0` pin is grounded. Wire `AD0` to `3.3V` immediately to prevent collision!

### Step 3: Sensor Data Verification
- Type `readimu`. Verify valid acceleration (~$9.81\,m/s^2$ on Z axis) and gyro values.
- Type `readtemp`. Verify RTD temperature (~$20^\circ C - 30^\circ C$ ambient).
- Type `readmag`. Verify magnetic vector (~$20-60\,\mu T$ magnitude).
- Type `readrtc`. Verify RTC timestamp matches system date/time.

### Step 4: Pulse Induction Bench Test
- Type `piread`. Verify baseline decay curve readings.
- Bring a metallic object (steel key or coin) near the PI coil.
- Type `piread`. Verify `pi_strength` increases significantly and `pi_detected` flips to `1`.
