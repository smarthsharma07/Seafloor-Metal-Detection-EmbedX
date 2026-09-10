# Requirements Traceability Matrix

This document maps all SIH project requirements to firmware implementation modules, input/output interfaces, and verification statuses.

---

## 1. Traceability Matrix

| Req ID | Requirement Description | Implementation Module | Primary Input | Primary Output | Verification Method | Status |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **REQ-DRV-01** | MPU6050 Motion Sensing & Die Temp | `src/sensors/mpu6050.h/cpp` | I2C Bus (`0x69` default) | Accel ($\text{m}/\text{s}^2$), Gyro ($\text{rad}/\text{s}$), Die Temp ($^\circ\text{C}$), `mpu_valid` | Startup Identity Check (`WHO_AM_I=0x68`), Static Code Review | **[SOURCE VERIFIED]** (Pending Bench Test) |
| **REQ-DRV-02** | DS3231 RTC Timestamping | `src/sensors/ds3231.h/cpp` | I2C Bus (`0x68` fixed) | Unix Epoch, ISO-8601 String, `rtc_valid` | Range Sanity Check & Status Probe | **[SOURCE VERIFIED]** (Pending Bench Test) |
| **REQ-DRV-03** | MAX31865 Seawater Temperature | `src/sensors/max31865_pt100.h/cpp` | SPI Bus (`GPIO5` CS, Mode 1) | RTD Ohms, Seawater Temp ($^\circ\text{C}$), `rtd_valid` | Quadratic CVD Math & Config Readback | **[SOURCE VERIFIED]** (Pending Bench Test) |
| **REQ-DRV-04** | RM3100 3-Axis Magnetometer | `src/sensors/rm3100.h/cpp` | SPI Bus (`GPIO4` CS, Mode 0) | Magnetic Vector ($\mu\text{T}$), `mag_valid` | REVID Check (`0x36`) & CC Readback | **[SOURCE VERIFIED]** (Pending Bench Test) |
| **REQ-PI-01** | Parameterized PI Anomaly Detector | `src/pi/pi_detector.h/cpp` | `GPIO33` Gate, `GPIO32` ADC1_CH4 | Raw Decay[16], Baseline, Peak, Early/Late, Strength, `pi_valid` | Multi-burst Acquisition & Sigma Derivation | **[PROVISIONAL]** (Pending Coil Bench Test) |
| **REQ-FUS-01** | IMU Attitude Filter (Roll/Pitch) | `src/fusion/attitude.h/cpp` | Calibrated Accel & Gyro | Roll ($\phi$), Pitch ($\theta$) | Mathematical Simulation | **[SOURCE VERIFIED]** |
| **REQ-FUS-02** | Tilt-Compensated Heading | `src/fusion/heading.h/cpp` | Calibrated Mag + Roll/Pitch | Heading ($\psi$, $0-360^\circ$), Calibrated $\mu\text{T}$ | Synthetic Vector Test (`test_fusion_math.py`) | **[SIMULATION VERIFIED]** |
| **REQ-FUS-03** | Conditional Drift Sector Fusion | `src/fusion/drift_fusion.h/cpp` | Mag Heading + IMU Motion | `accel_horiz_mag`, `imu_direction_valid`, `drift_bearing`, `drift_radius`, `uncertainty_deg` | Synthetic Vector Test (`test_fusion_math.py`) | **[SIMULATION VERIFIED]** |
| **REQ-FUS-04** | Separate Confidence Metrics | `src/fusion/drift_fusion.h/cpp` | Field Magnitude & Motion SNR | `mag_conf`, `imu_conf`, `fusion_conf` | Confidence Metric Analysis | **[SIMULATION VERIFIED]** |
| **REQ-LOG-01** | Telemetry Data Logging | `src/logging/logger.h/cpp` | Unified `SensorSample` | LittleFS CSV File (`/telemetry.csv`) | Storage Schema Review | **[SOURCE VERIFIED]** |
| **REQ-COM-01** | Recovery Wi-Fi Server | `src/communication/wifi_manager.h/cpp` | LittleFS Files | Endpoints: `/`, `/status`, `/config`, `/calibration`, `/files`, `/download` | HTTP Route Handler Review | **[SOURCE VERIFIED]** |
| **REQ-SIM-01** | DEMO_MODE Simulation Engine | `src/main.cpp` | Synthetic Timers | Full Simulated Telemetry Pipeline | End-to-End Simulation Step | **[SOURCE VERIFIED]** |
| **REQ-DIAG-01**| Boot Diagnostics & CLI | `src/diagnostics/diagnostics.h/cpp` & `src/main.cpp` | Sensor Init Status & CLI | Self-Test Table, CLI (`piread`, `pical`, `magcal`, `imucal`, `demo`) | Interactive CLI Review | **[SOURCE VERIFIED]** |

---

## 2. Verification Classification Standard

1. **`[SOURCE VERIFIED]`**: Code structure, driver registers, error handling, and syntax verified via code review and static analysis.
2. **`[SIMULATION VERIFIED]`**: Mathematical algorithms (attitude filter, heading projection, conditional IMU motion, drift fusion) validated against synthetic test vectors (`test/test_fusion_math.py`).
3. **`[PROVISIONAL / BENCH_TBD]`**: Hardware interactions (PI coil decay curves, RTD reference resistor, SCL pin continuity) requiring physical bench testing.
4. **`[BENCH HARDWARE VERIFIED]`**: Validated on physical electronics workbench with actual breakout boards. *(PENDING).*
5. **`[FIELD VERIFIED]`**: Confirmed during open-ocean sea trials and drift deployment experiments. *(PENDING).*
