#ifndef MPU6050_H
#define MPU6050_H

#include <Arduino.h>
#include <Wire.h>
#include "config.h"

class MPU6050Sensor {
public:
    MPU6050Sensor(uint8_t i2cAddr = MPU6050_I2C_ADDR_DEFAULT);

    bool begin(TwoWire &wireBus = Wire, uint8_t i2cAddr = 0);
    bool readRaw(int16_t &ax, int16_t &ay, int16_t &az, int16_t &gx, int16_t &gy, int16_t &gz, int16_t &tempRaw);
    bool readScaled(float &ax_m_s2, float &ay_m_s2, float &az_m_s2,
                    float &gx_rad_s, float &gy_rad_s, float &gz_rad_s,
                    float &dieTemp_C);

    void setCalibration(const ImuCalibrationData &cal);
    const ImuCalibrationData& getCalibration() const { return _calData; }

    bool isHealthy() const { return _isInitialized; }
    uint8_t getAddress() const { return _i2cAddr; }
    void printDiagnostics() const;

private:
    TwoWire *_wire;
    uint8_t _i2cAddr;
    bool _isInitialized;
    ImuCalibrationData _calData;

    void writeRegister(uint8_t reg, uint8_t value);
    uint8_t readRegister(uint8_t reg);
};

#endif // MPU6050_H
