# Engineering Corrective Review & Gap Analysis

This document records the systematic review of the SIH seafloor metal-detection sensor package codebase and documentation, identifying discrepancies, severity levels, root causes, proposed fixes, and hardware verification requirements.

---

## 1. Discrepancy & Defect Register

| ID | Category | Issue Description | Severity | Evidence in Code / Docs | Proposed Engineering Fix | Hardware Verification Required? |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **CR-01** | Hardware / Pinout | Premature declaration of GPIO pinout as confirmed without physical continuity test. | **HIGH** | `HARDWARE_TRUTH_TABLE.md` listed pins as CONFIRMED based on Olimex reference diagrams. | Reclassify all pins as `PROVISIONAL / BENCH_TBD` until physical continuity and board revision are confirmed. | **YES** |
| **CR-02** | I2C Protocol / Safety | MPU6050 fallback probe to `0x68` creates unsafe bus collision with DS3231 RTC. | **CRITICAL** | `mpu6050.cpp` probed `0x68` if `0x69` failed, potentially driving DS3231. | Initialize DS3231 at `0x68` first. Strictly enforce MPU6050 at `0x69`. Abort with clear error if `0x69` is missing. | **YES** |
| **CR-03** | Data Model | Temperature collision: MPU6050 internal die temperature overwrote PT100 seawater temperature. | **CRITICAL** | `main.cpp` passed `g_latestSample.temperature_C` to `g_mpu.readScaled()`, overwriting PT100 reading. | Separate into `water_temperature_C` (MAX31865 PT100) and `imu_temperature_C` (MPU6050 die temp) in `SensorSample`. | **NO** (Software fix) |
| **CR-04** | Serial CLI | `piread` command only printed stale cached telemetry rather than triggering an active measurement. | **MEDIUM** | `main.cpp` printed `g_latestSample.pi_response_strength` without invoking `g_pi.processPulseSequence()`. | Execute a live PI pulse sequence within `piread` CLI handler and print real-time decay and delta counts. | **YES** (Bench verification) |
| **CR-05** | Calibration CLI | `magcal`, `imucal`, and `pical` routines were documented but lacked implementation in CLI. | **HIGH** | `main.cpp` lacked logic for multi-point mag sampling and stationary gyro offset averaging. | Implement multi-sample interactive calibration loops in `main.cpp` and update `HeadingCalculator` / `AttitudeFilter`. | **YES** (Bench verification) |
| **CR-06** | Web Recovery API | Endpoints `/config` and `/calibration` were documented in help/docs but not bound in `wifi_manager.cpp`. | **MEDIUM** | `wifi_manager.cpp` only had `/`, `/status`, `/files`, `/download`. | Implement dedicated `/config` (JSON) and `/calibration` (JSON) endpoints in `WiFiRecoveryManager`. | **NO** (Software fix) |
| **CR-07** | Logging Claims | Documentation claimed "binary log format" support when only CSV logging is implemented. | **LOW** | `DATA_FORMAT.md` & `ARCHITECTURE.md` mentioned binary format. | Remove all binary logging claims. Explicitly document LittleFS CSV logging as the sole storage format. | **NO** (Doc fix) |
| **CR-08** | RTD Math | Linear Callendar-Van Dusen equation used ($R(T) = R_0(1+AT)$), omitting second-order $B T^2$ term. | **MEDIUM** | `max31865_pt100.cpp` used linear equation while docs claimed full CVD. | Implement quadratic Callendar-Van Dusen formula for $T \ge 0^\circ\text{C}$ and accurately document the calculation. | **NO** (Math fix) |
| **CR-09** | RTD Hardware | $R_{ref} = 430.0\,\Omega$ was hardcoded without measuring the physical 7Semi board resistor. | **HIGH** | `config.h` defined `MAX31865_RREF_OHMS 430.0f`. | Maintain configurable $R_{ref}$ and explicitly mark as `UNVERIFIED / BENCH_TBD`. | **YES** |
| **CR-10** | PI Concurrency | PI pulse sequence claimed to be non-blocking but utilized microsecond delay loops. | **HIGH** | `pi_detector.cpp` used synchronous `delayMicroseconds()` loops. | Clarify that PI pulse bursts require microsecond-level timing and implement non-blocking inter-pulse state scheduling. | **YES** |
| **CR-11** | RM3100 Gain | Fixed 75 LSB/uT gain assumed cycle count of 200 without runtime verification. | **MEDIUM** | `rm3100.cpp` assumed default 200 counts without verifying register write success. | Verify cycle count register readback during `begin()` before applying 75 LSB/uT gain factor. | **YES** |
| **CR-12** | Drift Fusion Claims | Documentation risked implying 400 m dead reckoning. | **HIGH** | Various docs needed clearer distinction between drift sector estimation and dead reckoning. | Emphasize throughout all docs that drift fusion is a statistical uncertainty sector model, NOT dead reckoning. | **NO** (Doc fix) |
| **CR-13** | Traceability | `REQUIREMENTS_MATRIX.md` claimed `[BUILD VERIFIED]` before environment build execution. | **MEDIUM** | Requirements matrix marked statuses optimistically. | Re-evaluate verification statuses honestly against strict build and simulation criteria. | **NO** (Doc fix) |

---

## 2. Corrective Action Plan

1. **Step 1:** Update `src/config.h` with separated temperature fields, unverified markers, and clean calibration structs.
2. **Step 2:** Refactor `src/sensors/mpu6050.cpp` to eliminate `0x68` fallback when DS3231 is active.
3. **Step 3:** Implement quadratic Callendar-Van Dusen equation and register readback verification in `src/sensors/max31865_pt100.cpp`.
4. **Step 4:** Implement cycle count verification in `src/sensors/rm3100.cpp`.
5. **Step 5:** Refactor `src/pi/pi_detector.cpp` with honest timing documentation and non-blocking pulse acquisition cycle.
6. **Step 6:** Implement full interactive `magcal`, `imucal`, and `pical` routines and live `piread` in `src/main.cpp`.
7. **Step 7:** Implement `/config` and `/calibration` endpoints in `src/communication/wifi_manager.cpp`.
8. **Step 8:** Reconcile all 15 documentation files to remove unimplemented features (e.g. binary logging) and accurately describe approximations, calibration workflows, and provisional parameters.
