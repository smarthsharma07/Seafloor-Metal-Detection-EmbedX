#include "ds3231.h"
#include <time.h>

#define DS3231_REG_TIME   0x00
#define DS3231_REG_STATUS 0x0F

DS3231RTC::DS3231RTC() : _wire(&Wire), _i2cAddr(DS3231_I2C_ADDR), _isInitialized(false) {}

bool DS3231RTC::begin(TwoWire &wireBus, uint8_t i2cAddr) {
    _wire = &wireBus;
    _i2cAddr = i2cAddr;
    _isInitialized = false;

    // Sanity check 1: Probe status register
    _wire->beginTransmission(_i2cAddr);
    _wire->write(DS3231_REG_STATUS);
    if (_wire->endTransmission() != 0) {
        Serial.printf("[DS3231] ERROR: RTC device not responding at I2C Addr 0x%02X\n", _i2cAddr);
        return false;
    }

    if (_wire->requestFrom(_i2cAddr, (uint8_t)1) < 1) {
        Serial.println("[DS3231] ERROR: Failed to read status byte");
        return false;
    }
    uint8_t status = _wire->read();

    // Sanity check 2: Perform a test time read and verify logical ranges
    uint32_t testEpoch = 0;
    char testIso[32] = {0};
    _isInitialized = true; // Temporarily set to allow readTime check
    if (!readTime(testEpoch, testIso, sizeof(testIso))) {
        Serial.println("[DS3231] ERROR: Time registers failed sanity check range validation");
        _isInitialized = false;
        return false;
    }

    Serial.printf("[DS3231] SUCCESS: RTC Initialized at I2C Address 0x%02X (Status=0x%02X, Time: %s)\n",
                  _i2cAddr, status, testIso);
    return true;
}

bool DS3231RTC::readTime(uint32_t &epochSeconds, char *isoBuffer, size_t bufferLen) {
    if (!_isInitialized) return false;

    _wire->beginTransmission(_i2cAddr);
    _wire->write(DS3231_REG_TIME);
    if (_wire->endTransmission(false) != 0) return false;

    if (_wire->requestFrom(_i2cAddr, (uint8_t)7) < 7) return false;

    uint8_t sec  = bcd2dec(_wire->read() & 0x7F);
    uint8_t min  = bcd2dec(_wire->read() & 0x7F);
    uint8_t hour = bcd2dec(_wire->read() & 0x3F);
    _wire->read(); // Skip day of week
    uint8_t mday = bcd2dec(_wire->read() & 0x3F);
    uint8_t mon  = bcd2dec(_wire->read() & 0x1F);
    uint16_t year = 2000 + bcd2dec(_wire->read());

    // Sanity boundary checks on time fields
    if (sec > 59 || min > 59 || hour > 23 || mday == 0 || mday > 31 || mon == 0 || mon > 12 || year < 2020 || year > 2099) {
        return false;
    }

    struct tm tm_time;
    tm_time.tm_sec  = sec;
    tm_time.tm_min  = min;
    tm_time.tm_hour = hour;
    tm_time.tm_mday = mday;
    tm_time.tm_mon  = mon - 1;
    tm_time.tm_year = year - 1900;
    tm_time.tm_isdst = 0;

    epochSeconds = (uint32_t)mktime(&tm_time);

    if (isoBuffer != nullptr && bufferLen > 0) {
        snprintf(isoBuffer, bufferLen, "%04u-%02u-%02uT%02u:%02u:%02uZ",
                 year, mon, mday, hour, min, sec);
    }

    return true;
}

bool DS3231RTC::setTime(uint32_t epochSeconds) {
    if (!_isInitialized) return false;

    time_t t = (time_t)epochSeconds;
    struct tm *tm_time = gmtime(&t);
    if (!tm_time) return false;

    _wire->beginTransmission(_i2cAddr);
    _wire->write(DS3231_REG_TIME);
    _wire->write(dec2bcd(tm_time->tm_sec));
    _wire->write(dec2bcd(tm_time->tm_min));
    _wire->write(dec2bcd(tm_time->tm_hour));
    _wire->write(dec2bcd(tm_time->tm_wday + 1));
    _wire->write(dec2bcd(tm_time->tm_mday));
    _wire->write(dec2bcd(tm_time->tm_mon + 1));
    _wire->write(dec2bcd((uint8_t)(tm_time->tm_year % 100)));
    return (_wire->endTransmission() == 0);
}

void DS3231RTC::printDiagnostics() const {
    Serial.printf("  DS3231 RTC    : Status=%s | Addr=0x%02X\n",
                  _isInitialized ? "OK" : "FAULT", _i2cAddr);
}

uint8_t DS3231RTC::bcd2dec(uint8_t val) {
    return ((val / 16 * 10) + (val % 16));
}

uint8_t DS3231RTC::dec2bcd(uint8_t val) {
    return ((val / 10 * 16) + (val % 10));
}
