#ifndef DS3231_H
#define DS3231_H

#include <Arduino.h>
#include <Wire.h>
#include "config.h"

class DS3231RTC {
public:
    DS3231RTC();

    bool begin(TwoWire &wireBus = Wire, uint8_t i2cAddr = DS3231_I2C_ADDR);
    bool readTime(uint32_t &epochSeconds, char *isoBuffer, size_t bufferLen);
    bool setTime(uint32_t epochSeconds);

    bool isHealthy() const { return _isInitialized; }
    void printDiagnostics() const;

private:
    TwoWire *_wire;
    uint8_t _i2cAddr;
    bool _isInitialized;

    uint8_t bcd2dec(uint8_t val);
    uint8_t dec2bcd(uint8_t val);
};

#endif // DS3231_H
