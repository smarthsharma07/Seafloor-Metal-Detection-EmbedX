#ifndef DIAGNOSTICS_H
#define DIAGNOSTICS_H

#include <Arduino.h>
#include "sensors/mpu6050.h"
#include "sensors/ds3231.h"
#include "sensors/max31865_pt100.h"
#include "sensors/rm3100.h"
#include "pi/pi_detector.h"
#include "logging/logger.h"
#include "communication/wifi_manager.h"

class SystemDiagnostics {
public:
    static void printSelfTestTable(const MPU6050Sensor &mpu,
                                   const DS3231RTC &rtc,
                                   const MAX31865Sensor &rtd,
                                   const RM3100Sensor &mag,
                                   const PIDetector &pi,
                                   const TelemetryLogger &logger,
                                   const WiFiRecoveryManager &wifi);

    static void scanI2CBus(TwoWire &wireBus = Wire);
};

#endif // DIAGNOSTICS_H
