# Calibration Procedure Guide

This document specifies the step-by-step procedures for calibrating the RM3100 magnetometer, MPU6050 IMU, and Pulse Induction baseline engine via the interactive serial CLI.

---

## 1. RM3100 Magnetometer Calibration (`magcal`)

Magnetic fields from onboard batteries, wiring, and structural metals introduce hard-iron bias and soft-iron scale distortion.

### Procedure:
1. Connect the ESP32 to a serial monitor at 115200 baud.
2. Enter command: `magcal`
3. Slowly rotate the sensor package through a continuous 3D figure-8 motion covering all spatial orientations (pitch, roll, yaw) for 15 seconds.
4. The system collects live magnetic samples and records axis extrema ($X_{\min}, X_{\max}, Y_{\min}, Y_{\max}, Z_{\min}, Z_{\max}$).
5. The calibration algorithm calculates the sphere center offset and scale multipliers:
   $$V_x = \frac{X_{\max} + X_{\min}}{2}, \quad \text{Scale}_x = \frac{\Delta_{\text{avg}}}{\Delta_x}$$
6. Offsets and scale multipliers are immediately applied to the active `HeadingCalculator` instance.

---

## 2. MPU6050 IMU Zero-Motion Calibration (`imucal`)

MEMS gyroscopes exhibit zero-rate bias drift, and accelerometers require level calibration.

### Procedure:
1. Place the sensor package on a stable, level surface.
2. Enter command: `imucal`
3. Keep the unit completely motionless for 4 seconds while 200 samples are accumulated.
4. The system calculates zero-rate gyro offsets and accelerometer bias (adjusting $Z$-axis for $1g = 9.80665\,\text{m}/\text{s}^2$).
5. Calibration offsets are applied to the active `MPU6050Sensor` driver.

---

## 3. Pulse Induction Baseline Zeroing (`pical`)

Background mineralized seabed soils and ambient electromagnetic conditions affect the zero-metal decay curve.

### Procedure:
1. Position the sensor in an environment free of metallic target objects.
2. Enter command: `pical`
3. The system resets the decay baseline to zero and executes a sequence of 10 pulse bursts to establish the background decay level.
