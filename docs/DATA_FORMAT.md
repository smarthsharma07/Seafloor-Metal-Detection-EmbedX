# Data Format Specification

This document details the telemetry logging CSV file format and the JSON schemas served by the Wi-Fi Recovery web server.

---

## 1. Local LittleFS CSV Telemetry Log (`/telemetry.csv`)

Telemetry is written directly to the onboard LittleFS filesystem as a standardized CSV file.

### CSV Header Definition
```csv
timestamp,water_temp_C,imu_temp_C,rtd_ohms,accel_x,accel_y,accel_z,gyro_x,gyro_y,gyro_z,mag_x,mag_y,mag_z,heading,roll,pitch,pi_strength,pi_detected,drift_bearing,confidence
```

### Sample Record
```csv
2026-09-07T14:30:00.000Z,24.15,31.20,109.42,0.02,-0.01,9.81,0.001,-0.002,0.000,21.50,-12.30,42.10,145.20,0.12,-0.06,12.4,0,145.20,0.85
```

### Column Field Descriptions
| Column Index | Field Name | Unit | Type | Description |
| :--- | :--- | :--- | :--- | :--- |
| 1 | `timestamp` | ISO-8601 UTC | String | Formatted UTC timestamp retrieved from DS3231 RTC. |
| 2 | `water_temp_C` | $^\circ\text{C}$ | Float | Seawater temperature measured by MAX31865 PT100 probe. |
| 3 | `imu_temp_C` | $^\circ\text{C}$ | Float | Internal MPU6050 die temperature. |
| 4 | `rtd_ohms` | $\Omega$ | Float | Measured RTD probe resistance. |
| 5-7 | `accel_x, accel_y, accel_z` | $\text{m}/\text{s}^2$ | Float | Calibrated 3-axis acceleration. |
| 8-10 | `gyro_x, gyro_y, gyro_z` | $\text{rad}/\text{s}$ | Float | Calibrated 3-axis angular rates. |
| 11-13 | `mag_x, mag_y, mag_z` | $\mu\text{T}$ | Float | Calibrated 3-axis magnetic field vector. |
| 14 | `heading` | Degrees ($0-360^\circ$) | Float | Tilt-compensated magnetic heading. |
| 15-16 | `roll, pitch` | Degrees | Float | Computed vessel attitude from complementary filter. |
| 17 | `pi_strength` | ADC Counts | Float | Pulse Induction decay anomaly magnitude above baseline. |
| 18 | `pi_detected` | Boolean ($0/1$) | Integer | 1 if `pi_strength` exceeds threshold; 0 otherwise. |
| 19 | `drift_bearing` | Degrees ($0-360^\circ$) | Float | Estimated current-aligned dominant drift bearing. |
| 20 | `confidence` | Scale ($0.0 - 1.0$) | Float | Directional sector fusion confidence score. |

---

## 2. Recovery Wi-Fi JSON API Endpoints

### A. Live Status API (`GET /status`)
```json
{
  "timestamp": "2026-09-07T14:30:00.000Z",
  "water_temperature_C": 24.15,
  "imu_temperature_C": 31.20,
  "rtd_ohms": 109.42,
  "accel": {"x": 0.02, "y": -0.01, "z": 9.81},
  "gyro": {"x": 0.001, "y": -0.002, "z": 0.000},
  "mag_uT": {"x": 21.50, "y": -12.30, "z": 42.10},
  "heading_deg": 145.20,
  "attitude": {"roll_deg": 0.12, "pitch_deg": -0.06},
  "pi_detector": {
    "strength": 12.4,
    "detected": false,
    "baseline": 105.2
  },
  "drift_fusion": {
    "bearing_deg": 145.20,
    "uncertainty_deg": 30.0,
    "radius_m": 400.0,
    "confidence": 0.85
  }
}
```

### B. Configuration API (`GET /config`)
```json
{
  "i2c": {"sda": 13, "scl": 16, "speed": 400000, "mpu_addr": "0x69", "ds3231_addr": "0x68"},
  "spi": {"sck": 14, "miso": 2, "mosi": 15, "rm3100_cs": 4, "max31865_cs": 5},
  "pi": {"gate_gpio": 33, "adc_gpio": 32, "width_us": 150, "settle_us": 30, "period_us": 1000, "threshold": 50.0},
  "fusion": {"w_mag": 0.80, "w_imu": 0.20, "drift_radius_m": 400.0, "uncertainty_deg": 30.0}
}
```

### C. Calibration API (`GET /calibration`)
```json
{
  "mag": {
    "offset": {"x": 2.15, "y": -1.40, "z": 5.80},
    "scale": {"x": 1.02, "y": 0.98, "z": 1.00},
    "calibrated": true
  },
  "imu": {
    "accel_offset": {"x": 0.015, "y": -0.020, "z": 0.005},
    "gyro_offset": {"x": 0.0012, "y": -0.0008, "z": 0.0002},
    "calibrated": true
  }
}
```
