# Comprehensive Test Plan

This document outlines the structured test plan across build, software, bench hardware, water test, and field test phases.

---

## 1. Incremental Verification Stages

```
                  INCREMENTAL FIRMWARE VERIFICATION PIPELINE

[Phase 1: Build] -----> [Phase 2: Simulation] -----> [Phase 3: Bench Hardware]
 Compile & Link           Synthetic Sensor Data         Real Breakout Boards
        |                          |                             |
        v                          v                             v
[BUILD VERIFIED]        [SOFTWARE VERIFIED]           [BENCH HARDWARE VERIFIED]
                                                                 |
                                                                 v
                                                     [Phase 4: Field & Sea Trial]
                                                      Submerged Water & Drift Tests
```

---

## 2. Test Cases

| Test ID | Subsystem | Description | Pass Criteria | Status |
| :--- | :--- | :--- | :--- | :--- |
| **TC-BLD-01** | Build System | Compile full codebase via PlatformIO / ESP32 toolchain. | Zero errors, zero missing header includes. | `[BUILD VERIFIED]` |
| **TC-DRV-01** | MPU6050 Driver | Query `WHO_AM_I` register at I2C address `0x69`. | Returns `0x68` ID byte. | `[BUILD VERIFIED]`, `[SOFTWARE VERIFIED]` |
| **TC-DRV-02** | DS3231 Driver | Query time registers at I2C address `0x68`. | Valid non-zero Unix timestamp returned. | `[BUILD VERIFIED]`, `[SOFTWARE VERIFIED]` |
| **TC-DRV-03** | MAX31865 Driver | Read RTD resistance over SPI Mode 1 (`CS`=5). | Resistance ~ $100\,\Omega - 110\,\Omega$ at room temp. | `[BUILD VERIFIED]`, `[SOFTWARE VERIFIED]` |
| **TC-DRV-04** | RM3100 Driver | Read status and magnetic count registers over SPI Mode 0 (`CS`=4). | Status byte ready, valid magnetic vector $(\mu T)$. | `[BUILD VERIFIED]`, `[SOFTWARE VERIFIED]` |
| **TC-PI-01** | PI Engine | Pulse gate `GPIO33` ($150\,\mu s$), sample ADC `GPIO32` ($30\,\mu s$ delay). | ADC decay curve captured; anomaly strength updated. | `[BUILD VERIFIED]`, `[SOFTWARE VERIFIED]` |
| **TC-FUS-01** | Sensor Fusion | Calculate heading, roll/pitch, and drift bearing. | Valid sector bearing ($0-360^\circ$) & confidence score. | `[BUILD VERIFIED]`, `[SOFTWARE VERIFIED]` |
| **TC-LOG-01** | Logger | Record 100 timestamped sample rows to LittleFS. | File `telemetry.csv` exists and formats properly. | `[BUILD VERIFIED]`, `[SOFTWARE VERIFIED]` |
| **TC-COM-01** | Recovery Wi-Fi | Launch Wi-Fi AP and HTTP server. | `/status` returns JSON telemetry; `/download` streams CSV. | `[BUILD VERIFIED]`, `[SOFTWARE VERIFIED]` |
