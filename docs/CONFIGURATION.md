# Configuration & Parameters Guide

This document describes all centralized configuration macros, parameter structures, default values, and runtime modification interfaces.

---

## 1. Global Configuration Structure (`src/config.h`)

### Hardware Pin Definitions
```cpp
#define I2C_SDA_PIN         13      // EXT2 Pin 3
#define I2C_SCL_PIN         16      // EXT2 Pin 4 (HARDWARE_TBD)
#define SPI_MOSI_PIN        15      // EXT2 Pin 8
#define SPI_MISO_PIN        2       // EXT1 Pin 3
#define SPI_SCK_PIN         14      // EXT2 Pin 7
#define RM3100_CS_PIN       4       // EXT1 Pin 4
#define MAX31865_CS_PIN     5       // EXT1 Pin 5
#define PI_MOSFET_GATE_PIN  33      // EXT2 Pin 5
#define PI_ADC_INPUT_PIN    32      // EXT2 Pin 6 (ADC1_CH4)
```

### Sensor Settings & Bus Speeds
```cpp
#define I2C_BUS_SPEED       400000U // 400 kHz Fast-mode I2C
#define MPU6050_I2C_ADDR    0x69    // Address with AD0 pulled HIGH
#define DS3231_I2C_ADDR     0x68    // Fixed RTC Address
#define MAX31865_RREF       430.0f  // Reference resistor (UNVERIFIED)
#define MAX31865_RNOMINAL   100.0f  // PT100 Nominal 0C resistance
```

### Pulse Induction Engine Parameters
```cpp
#define PI_PULSE_WIDTH_US   150     // PROVISIONAL
#define PI_SETTLING_DELAY_US 30     // PROVISIONAL
#define PI_PULSE_PERIOD_US  1000    // PROVISIONAL
#define PI_SAMPLE_COUNT     16      // Decay curve samples
#define PI_AVERAGING_COUNT  20      // Pulse averaging
```

---

## 2. Runtime CLI Configuration

All parameters can be queried and modified at runtime via the serial console interface using commands:
- `config` : Prints current parameter list.
- `set pitime <width_us> <settle_us> <period_us>` : Updates PI timing parameters.
- `set dithresh <threshold_counts>` : Updates PI metal anomaly detection threshold.
- `set drift <radius_m> <angle_deg>` : Updates drift radius and sector uncertainty angle.
