# Prototype Scope & Known Limitations

This document explicitly defines the engineering boundaries, prototype scope, and known physical/electrical/algorithmic limitations of the deployable seafloor metal-detection sensor package.

---

## 1. Prototype Scope

The primary objective of this SIH hardware prototype is to validate a low-cost deployable ocean-bottom sensor architecture capable of:
1. Detecting conductive seafloor metallic anomalies using a parameterized Pulse Induction (PI) coil.
2. Estimating a constrained directional drift sector ($\pm 30^\circ$ around dominant heading) from surface deployment coordinates rather than assuming an unconstrained 400 m circle.
3. Recording seawater temperature via a PT100 RTD interface and timestamping environmental data.
4. Providing local LittleFS CSV telemetry logging and post-recovery Wi-Fi data offload.

> [!IMPORTANT]
> **PROTOTYPE BOUNDARY:**
> - The prototype is designed for bench bring-up, pressure chamber testing, and near-shore prototype drift trials.
> - It is **NOT** a certified commercial deep-sea oceanographic instrument.

---

## 2. Known System Limitations

### A. Localization & Sensor Fusion Limitations
1. **No Inertial Dead Reckoning:**
   The MPU6050 6-axis MEMS IMU is used solely for tilt compensation and short-term horizontal acceleration detection. Accelerometer bias and integration drift prevent long-duration underwater dead reckoning. The system does not claim exact $(x, y)$ seabed coordinates.
2. **Passive Fin Dependency:**
   The directional drift estimate assumes the vessel's passive mechanical fins have hydrodynamically aligned with the dominant underwater current. There is no electronic fin angle sensor.
3. **Absence of Acoustic Positioning:**
   The system does not include a Doppler Velocity Log (DVL) or Ultra-Short Baseline (USBL) acoustic positioning transponder.

### B. Electrical & Hardware Limitations
4. **I2C Bus Contention:**
   The DS3231 RTC fixed address `0x68` collides with the MPU6050 default address. The MPU6050 `AD0` pin **must** be wired to $3.3\,\text{V}$ (address `0x69`).
5. **PI Coil Mutual Electromagnetic Interference:**
   High-current PI switching pulses severely distort local magnetic fields. The firmware enforces time-multiplexing to ensure the RM3100 magnetometer is never read during PI pulse firing.
6. **Internal ADC Non-Linearity:**
   The internal ESP32 12-bit ADC (`GPIO32` / `ADC1_CH4`) exhibits non-linearity near $0.0\,\text{V}$ and $3.3\,\text{V}$. Precise PI decay analysis requires baseline calibration.

### C. Operational Limitations
7. **Submerged Wi-Fi Inoperability:**
   2.4 GHz RF signals are absorbed by seawater. Wi-Fi recovery mode operates only after physical retrieval to the research vessel.
8. **Provisional Parameters:**
   PI pulse timings ($150\,\mu\text{s}$ width, $30\,\mu\text{s}$ settle) and MAX31865 $R_{ref}$ ($430\,\Omega$) are provisional defaults and must be tuned on the physical hardware.
