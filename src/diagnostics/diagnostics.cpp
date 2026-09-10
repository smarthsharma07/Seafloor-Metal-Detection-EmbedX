#include "diagnostics.h"

void SystemDiagnostics::printSelfTestTable(const MPU6050Sensor &mpu,
                                          const DS3231RTC &rtc,
                                          const MAX31865Sensor &rtd,
                                          const RM3100Sensor &mag,
                                          const PIDetector &pi,
                                          const TelemetryLogger &logger,
                                          const WiFiRecoveryManager &wifi) {
    Serial.println("\n=======================================================");
    Serial.println("         SYSTEM SELF-TEST HARDWARE STATUS TABLE");
    Serial.println("=======================================================");
    Serial.printf("  MPU6050 IMU   : %s  (I2C Addr: 0x%02X)\n", mpu.isHealthy() ? "OK    " : "FAILED", mpu.getAddress());
    Serial.printf("  DS3231 RTC    : %s  (I2C Addr: 0x68)\n", rtc.isHealthy() ? "OK    " : "FAILED");
    Serial.printf("  MAX31865 RTD  : %s  (SPI CS: GPIO%u, Rref: %.1f)\n", rtd.isHealthy() ? "OK    " : "FAILED", rtd.getCsPin(), rtd.getRref());
    Serial.printf("  RM3100 MAG    : %s  (SPI CS: GPIO%u, Mode 0)\n", mag.isHealthy() ? "OK    " : "FAILED", mag.getCsPin());
    Serial.printf("  PI DETECTOR   : %s  (Gate: GPIO33, ADC: GPIO32)\n", pi.isHealthy() ? "READY " : "FAILED");
    Serial.printf("  DATA LOGGER   : %s  (LittleFS Mounted)\n", logger.getLogFileSize() > 0 || logger.isHealthy() ? "READY " : "FAILED");
    Serial.printf("  WIFI RECOVERY : %s  (AP Mode Standing By)\n", wifi.isRunning() ? "ACTIVE" : "READY ");
    Serial.println("=======================================================\n");
}

void SystemDiagnostics::scanI2CBus(TwoWire &wireBus) {
    Serial.println("\n--- Scanning I2C Bus ---");
    uint8_t count = 0;
    for (uint8_t addr = 1; addr < 127; addr++) {
        wireBus.beginTransmission(addr);
        if (wireBus.endTransmission() == 0) {
            Serial.printf("  Found I2C device at address 0x%02X", addr);
            if (addr == 0x68) Serial.print(" (DS3231 RTC or MPU6050 AD0=GND)");
            if (addr == 0x69) Serial.print(" (MPU6050 AD0=3.3V)");
            Serial.println();
            count++;
        }
    }
    if (count == 0) {
        Serial.println("  No I2C devices found!");
    }
    Serial.println("--- Scan Complete ---\n");
}
