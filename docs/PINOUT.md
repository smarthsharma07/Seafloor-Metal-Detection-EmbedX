# Hardware Pinout & Wiring Specification

This document provides the verified pinout and physical header wiring mapping for the ESP32 deployment package.

---

## 1. ESP32 Extension Header Pin Assignments

```
               Olimex ESP32-POE / WROOM Extension Headers

         EXT1 (Left Header)                EXT2 (Right Header)
      +----------------------+          +----------------------+
      | 1 : 3.3V Power       |          | 1 : 5.0V Power       |
      | 2 : GND              |          | 2 : GND              |
      | 3 : GPIO2  (MISO)    |          | 3 : GPIO13 (I2C SDA) |
      | 4 : GPIO4  (RM CS)   |          | 4 : GPIO16 (I2C SCL)*|
      | 5 : GPIO5  (MAX CS)  |          | 5 : GPIO33 (PI Gate) |
      | 6 : GPIO18           |          | 6 : GPIO32 (PI ADC)  |
      | ...                  |          | 7 : GPIO14 (SPI SCK) |
      +----------------------+          | 8 : GPIO15 (SPI MOSI)|
                                        +----------------------+
```

*Note: SCL is mapped to EXT2 Pin 4 (`GPIO16`). Default configurable via `I2C_SCL_PIN` in `config.h`.

---

## 2. Bus Allocation Summary

### I2C Bus (`Wire`)
- **SDA:** `GPIO13` (EXT2 Pin 3)
- **SCL:** `GPIO16` (EXT2 Pin 4) - *HARDWARE_TBD*
- **Devices:**
  - `DS3231` RTC (Fixed address `0x68`)
  - `MPU6050` IMU (Address `0x69` when AD0 tied to 3.3V)

### SPI Bus (`SPI`)
- **MOSI:** `GPIO15` (EXT2 Pin 8)
- **MISO:** `GPIO2`  (EXT1 Pin 3)
- **SCK:**  `GPIO14` (EXT2 Pin 7)
- **Devices:**
  - `RM3100` Magnetometer (`CS` = `GPIO4`, SPI Mode 0, 1 MHz max)
  - `MAX31865` RTD Converter (`CS` = `GPIO5`, SPI Mode 1/3, 1 MHz max)

### Discrete Analog / GPIO
- **PI MOSFET Gate Trigger:** `GPIO33` (EXT2 Pin 5) - Digital Output
- **PI Signal Decay ADC Input:** `GPIO32` (EXT2 Pin 6) - `ADC1_CH4`
