# Firmware & Hardware Troubleshooting Guide

This document provides diagnostic solutions for common hardware failure modes, bus errors, and sensor operational issues.

---

## 1. Diagnostic Matrix

| Symptom / Fault | Potential Root Cause | Recommended Diagnostic Action |
| :--- | :--- | :--- |
| **MPU6050 missing on I2C scan** | `AD0` pin floating or grounded (`0x68` collision with DS3231). | Verify `AD0` tied to 3.3V (`0x69`). Check SDA/SCL pull-ups. |
| **MAX31865 reading -256.0°C / Fault 0xFF** | SPI CS pin not asserting or RTD disconnected. | Check SPI Mode (must be Mode 1 or 3). Inspect PT100 terminal block connection. |
| **RM3100 reading constant zeros** | `I2CEN` pin not grounded or wrong SPI mode. | Confirm `I2CEN` pin tied to GND. Verify SPI Mode 0 in transaction settings. |
| **PI coil triggers continuous false alarms** | Switching transient delay (`settling_delay_us`) too short. | Increase `settling_delay_us` from 30us to 50us. Re-run `pical` command. |
| **Wi-Fi AP fails to start** | Flash partition table full or brownout reset. | Check 5V power supply capacity. Verify LittleFS partition size. |
