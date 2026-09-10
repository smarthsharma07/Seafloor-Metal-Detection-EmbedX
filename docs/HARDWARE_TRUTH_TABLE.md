# Hardware Truth Table & Pre-Flight Review

This document establishes the hardware verification matrix for the deployable ocean-bottom sensor package based on direct inspection of supplied schematics, breakout board pinouts, and project architecture documents.

> [!CAUTION]
> **PRE-FLIGHT HARDWARE STATUS:** All GPIO assignments are currently classified as **PROVISIONAL / BENCH_TBD** pending physical electrical continuity checks and confirmation of the exact PCB revision.

---

## 1. Peripheral Hardware Matrix

| Function | Device | ESP32 GPIO | Physical Header Pin | Bus | Direction | Active Level | Voltage | Source Document | Verification Status | Engineering Notes |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **I2C SDA** | MPU6050 / DS3231 | `GPIO13` | EXT2 Pin 3 | I2C | Bidirectional | Open-Drain (4.7k Pull-up) | 3.3V | Project Wiring / EXT2 Map | **PROVISIONAL (BENCH_TBD)** | Shared I2C Data line. |
| **I2C SCL** | MPU6050 / DS3231 | `GPIO16` *(Default)* | EXT2 Pin 4 | I2C | Output | Open-Drain (4.7k Pull-up) | 3.3V | Olimex EXT2 Reference / Prompt `GPIO?` | **UNVERIFIED (BENCH_TBD)** | Prompt states `GPIO?`. EXT2 Pin 4 is GPIO16 on Olimex WROOM. |
| **SPI MISO** | RM3100 / MAX31865 | `GPIO2` | EXT1 Pin 3 | SPI | Input | Logic | 3.3V | Project Wiring / EXT1 Map | **PROVISIONAL (BENCH_TBD)** | Shared Master-In-Slave-Out line. |
| **SPI MOSI** | RM3100 / MAX31865 | `GPIO15` | EXT2 Pin 8 | SPI | Output | Logic | 3.3V | Project Wiring / EXT2 Map | **PROVISIONAL (BENCH_TBD)** | Shared Master-Out-Slave-In line. |
| **SPI SCK** | RM3100 / MAX31865 | `GPIO14` | EXT2 Pin 7 | SPI | Output | Logic | 3.3V | Project Wiring / EXT2 Map | **PROVISIONAL (BENCH_TBD)** | Shared Serial Clock line. |
| **RM3100 CS** | RM3100 Magnetometer | `GPIO4` | EXT1 Pin 4 | SPI CS | Output | Active LOW | 3.3V | Project Wiring / EXT1 Map | **PROVISIONAL (BENCH_TBD)** | Dedicated SPI Chip Select (SSN). SPI Mode 0. |
| **MAX31865 CS** | MAX31865 RTD | `GPIO5` | EXT1 Pin 5 | SPI CS | Output | Active LOW | 3.3V | Project Wiring / EXT1 Map | **PROVISIONAL (BENCH_TBD)** | Dedicated SPI Chip Select. SPI Mode 1. |
| **PI Trigger** | PI MOSFET Gate | `GPIO33` | EXT2 Pin 5 | GPIO | Output | Active HIGH | 3.3V -> Gate | Project Wiring / EXT2 Map | **PROVISIONAL (BENCH_TBD)** | Pulse Induction MOSFET gate pulse. |
| **PI ADC Input** | PI Decay Signal | `GPIO32` | EXT2 Pin 6 | ADC1_CH4 | Analog Input | Analog (0-3.3V) | 0-3.3V | Project Wiring / EXT2 Map | **PROVISIONAL (BENCH_TBD)** | ADC1 Channel 4; unaffected by Wi-Fi operations. |
| **DS3231 Address** | DS3231 RTC | N/A (I2C: `0x68`) | N/A | I2C | N/A | N/A | 3.3V | DS3231 Datasheet | **VERIFIED (I2C Standard)** | Fixed I2C slave address `0x68`. Initialized first in setup. |
| **MPU6050 Address** | MPU6050 IMU | N/A (I2C: `0x69`) | N/A | I2C | N/A | N/A | 3.3V | MPU6050 Schematic (Page 2) | **MANDATORY HARDWARE REQUIREMENT** | Breakout schematic shows R6 4.7k pull-down on AD0 (`0x68`). Must wire AD0 to 3.3V (`0x69`) to avoid bus collision. |
| **RM3100 I2CEN** | RM3100 Magnetometer | N/A (GND) | N/A | Discrete | Input | Active LOW (GND) | 0V | Sensor Schematics (Page 3) | **PROVISIONAL (BENCH_TBD)** | Must be tied to GND to force SPI protocol mode. |
| **MAX31865 $R_{ref}$**| 7Semi Board Resistor | N/A | PCB Surface | Analog | Precision | N/A | N/A | 7Semi Breakout PCB (Page 1) | **UNVERIFIED (BENCH_TBD)** | Default $430.0\,\Omega$. Must measure SMD resistor on physical board. |

---

## 2. Power and Grounding Architecture

- **Main Input:** $5.0\,\text{V}$ DC from step-down buck converter powered by $12\,\text{V}, 5\,\text{Ah}$ Li-Ion battery pack.
- **Sensor Logic Power:** $3.3\,\text{V}$ rail for MPU6050, DS3231, RM3100 (AVDD/DVDD), and MAX31865 (VIN/3V3).
- **Grounding Requirement:** Star-ground topology connecting digital logic ground, analog sensor ground, and MOSFET source return at a single point to prevent high-current PI induction switching transients from corrupting RTD and magnetometer readings.
