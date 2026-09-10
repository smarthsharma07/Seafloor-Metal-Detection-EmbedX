# Hardware Conflicts & Discrepancies Resolution Report

This document records all conflicts, discrepancies, and contradictions identified across project documents and hardware schematics, along with their engineering resolutions.

---

## 1. Discrepancy Matrix

| Discrepancy ID | Item | Source Document A | Source Document B | Engineering Resolution | Status |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **CONFLICT-01** | Magnetometer Part | PDF 1 & PDF 2 (MMC5983MA / QMC5883L) | PDF 3 & Prompt (RM3100) | Standardized on **RM3100** via SPI bus. QMC5883L/MMC5983MA code excluded. | **RESOLVED** |
| **CONFLICT-02** | I2C Address Collision | MPU6050 schematic (R6=4.7k to GND -> `0x68`) | DS3231 RTC datasheet (Fixed `0x68`) | MPU6050 `AD0` pin **must** be tied to 3.3V (`0x69`). Dynamic probe implemented. | **RESOLVED IN FIRMWARE** |
| **CONFLICT-03** | I2C SCL Pin Mapping | Project prompt text (`I2C SCL -> GPIO?`) | Olimex ESP32-POE EXT2 header (Pin 4 = `GPIO16`) | Set default `I2C_SCL_PIN` to `GPIO16` in `config.h`. Configurable via macro. | **HARDWARE_TBD** |
| **CONFLICT-04** | SPI Clock Mode Conflict | MAX31865 datasheet (Mode 1 / Mode 3, CPHA=1) | RM3100 datasheet (Mode 0, CPOL=0, CPHA=0) | Implemented per-device `SPISettings` before asserting Chip Select. | **RESOLVED IN FIRMWARE** |
| **CONFLICT-05** | MAX31865 $R_{ref}$ Value | Generic MAX31865 docs ($400.0\,\Omega$) | 7Semi Breakout PCB image (Page 1) | Set provisional default $R_{ref} = 430.0\,\Omega$. Marked `UNVERIFIED`. | **HARDWARE_TBD** |

---

## 2. Detailed Technical Evidence & Direct Quotes

### Conflict 01: Magnetometer Sensor Type
- **PDF 1 (Vessel Movement-Direction Prediction, Page 1):**
  > "Three Sources of Direction Information: MMC5983MA magnetometer - Measures the Earth's magnetic-field vector..."
- **PDF 2 (Project Proposal & Concept Note, Page 4):**
  > "Magnetometer: QMC5883L (Unconfirmed)"
- **PDF 3 (Sensor Schematics, Page 3):**
  > "Magnetometer – RM3100: SPI interface (SCK, SO, SI, SSN, DRDY, I2CEN=GND)"
- **User Request Instructions:**
  > "The latest hardware design uses RM3100. Do NOT implement QMC5883L support unless explicitly requested later."
- **Resolution:** `RM3100` driver implemented via SPI.

### Conflict 02: I2C Address Collision between MPU6050 and DS3231
- **PDF 3 (MPU6050 Schematic, Page 2):**
  > Circuit schematic shows resistor `R6 (4.7K)` pulling `AD0` pin to `GND`. When `AD0` is LOW, MPU6050 I2C 7-bit slave address is `0x68`.
- **DS3231 Datasheet Standard:**
  > Fixed 7-bit slave address `0x68`.
- **Engineering Risk:** If both chips share address `0x68` on the same I2C bus, bus contention occurs and both devices fail to read.
- **Resolution:**
  1. Hardware requirement: Tie MPU6050 `AD0` externally to `3.3V` (sets address to `0x69`).
  2. Software safety: `MPU6050` driver tests `0x69` first; if not detected, attempts `0x68` only if DS3231 probe is inactive.

### Conflict 03: I2C SCL Pin Mapping
- **User Prompt:**
  > `I2C: SDA -> GPIO13, SCL -> GPIO?`
- **Olimex ESP32-POE EXT2 Header Analysis:**
  > EXT2 Pin 3 = `GPIO13` (SDA)
  > EXT2 Pin 4 = `GPIO16` (SCL)
- **Resolution:** Set default `I2C_SCL_PIN` to `16` in `src/config.h`, but explicitly document as `HARDWARE_TBD` until physical bench continuity check is confirmed.
