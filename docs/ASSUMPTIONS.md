# Engineering Assumptions Document

This document lists every explicit engineering assumption made during the design and software architecture of the deployable seafloor metal-detection sensor package.

---

## 1. Hardware & Electrical Assumptions

1. **ESP32 Board Mapping:** Assumed Olimex ESP32-POE or compatible ESP32-WROOM-32E module pinout. `I2C_SCL_PIN` defaults to `GPIO16` (EXT2 Pin 4) as a provisional value, marked `UNVERIFIED BENCH_TBD`.
2. **I2C Pull-Up Resistors:** Assumed physical pull-up resistors ($4.7\,\text{k}\Omega$) are present on `SDA` (`GPIO13`) and `SCL` (`GPIO16`).
3. **MPU6050 Address Modification:** Mandatory requirement that MPU6050 `AD0` pin is tied to `3.3V` to force address `0x69`, avoiding hard bus collision with the DS3231 RTC (`0x68`).
4. **MAX31865 Reference Resistor:** Assumed 7Semi MAX31865 module uses a $430.0\,\Omega$ reference resistor ($R_{ref} = 430.0\,\Omega$) for PT100 elements. Marked `UNVERIFIED BENCH_TBD`.
5. **PT100 Configuration:** Assumed 2-wire PT100 RTD with Callendar-Van Dusen equation coefficients ($R_0 = 100.0\,\Omega, A = 3.9083 \times 10^{-3}, B = -5.775 \times 10^{-7}$).
6. **PI Coil Hardware Isolation:** Assumed the Pulse Induction MOSFET driver circuit includes flyback clamping diodes and voltage divider networks to clamp decay signals within the ESP32 `ADC1` range ($0.0\,\text{V} - 3.3\,\text{V}$).

---

## 2. Localization & Sensor Fusion Assumptions

7. **Passive Hydrodynamic Fin Alignment:** Assumed mechanical current-alignment fins passively rotate the cylindrical housing so its longitudinal axis aligns with the local current vector.
8. **No Electronic Fin Sensor:** Assumed there is **no electronic sensor** monitoring fin angle relative to the vessel. The magnetometer reading represents the current-aligned direction directly.
9. **Short-Term IMU Role:** Assumed MPU6050 accelerometer and gyroscope data are used exclusively for tilt compensation and short-term transient motion estimation, **not for long-duration 400 m underwater dead-reckoning**.
10. **Default Drift Scale:** Assumed nominal deployment-to-seafloor drift distance is $400\,\text{m}$ (configurable parameter).
