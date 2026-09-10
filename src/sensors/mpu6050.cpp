#include "mpu6050.h"

#define MPU6050_REG_SMPLRT_DIV   0x19
#define MPU6050_REG_CONFIG       0x1A
#define MPU6050_REG_GYRO_CONFIG  0x1B
#define MPU6050_REG_ACCEL_CONFIG 0x1C
#define MPU6050_REG_ACCEL_XOUT_H 0x3B
#define MPU6050_REG_PWR_MGMT_1   0x6B
#define MPU6050_REG_WHO_AM_I     0x75

MPU6050Sensor::MPU6050Sensor(uint8_t i2cAddr)
    : _wire(&Wire), _i2cAddr(i2cAddr), _isInitialized(false) {
    _calData = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, false};
}

bool MPU6050Sensor::begin(TwoWire &wireBus, uint8_t i2cAddr) {
    _wire = &wireBus;
    if (i2cAddr != 0) {
        _i2cAddr = i2cAddr;
    }
    _isInitialized = false;

    // Sanity check 1: Read WHO_AM_I register (must return 0x68)
    uint8_t chipId = readRegister(MPU6050_REG_WHO_AM_I);
    if (chipId != 0x68) {
        Serial.printf("[MPU6050] ERROR: Identity check failed at I2C Addr 0x%02X (WHO_AM_I=0x%02X, expected 0x68)\n",
                      _i2cAddr, chipId);
        if (_i2cAddr == 0x69) {
            Serial.println("[MPU6050] HARDWARE FAULT: AD0 pin might not be pulled HIGH to 3.3V.");
        }
        return false;
    }

    // Wake up device (clear SLEEP bit, select internal 8MHz clock)
    writeRegister(MPU6050_REG_PWR_MGMT_1, 0x00);
    delay(10);

    // Sanity check 2: Verify PWR_MGMT_1 cleared sleep mode
    uint8_t pwrMgmt = readRegister(MPU6050_REG_PWR_MGMT_1);
    if (pwrMgmt & 0x40) { // Sleep bit still set
        Serial.printf("[MPU6050] ERROR: Failed to clear SLEEP bit in PWR_MGMT_1 (0x%02X)\n", pwrMgmt);
        return false;
    }

    // Configure Sample Rate Divider: 1 kHz / (1 + 7) = 125 Hz
    writeRegister(MPU6050_REG_SMPLRT_DIV, 0x07);

    // Configure Digital Low-Pass Filter (DLPF): 42 Hz bandwidth
    writeRegister(MPU6050_REG_CONFIG, 0x03);

    // Configure Gyro Range: +/- 250 deg/s (131 LSB / deg/s)
    writeRegister(MPU6050_REG_GYRO_CONFIG, 0x00);

    // Configure Accel Range: +/- 2g (16384 LSB / g)
    writeRegister(MPU6050_REG_ACCEL_CONFIG, 0x00);

    _isInitialized = true;
    Serial.printf("[MPU6050] SUCCESS: IMU Initialized and Verified at I2C Address 0x%02X\n", _i2cAddr);
    return true;
}

void MPU6050Sensor::setCalibration(const ImuCalibrationData &cal) {
    _calData = cal;
    Serial.println("[MPU6050] Calibration offsets updated.");
}

bool MPU6050Sensor::readRaw(int16_t &ax, int16_t &ay, int16_t &az,
                            int16_t &gx, int16_t &gy, int16_t &gz,
                            int16_t &tempRaw) {
    if (!_isInitialized) return false;

    _wire->beginTransmission(_i2cAddr);
    _wire->write(MPU6050_REG_ACCEL_XOUT_H);
    if (_wire->endTransmission(false) != 0) return false;

    uint8_t count = _wire->requestFrom(_i2cAddr, (uint8_t)14);
    if (count < 14) return false;

    ax = (_wire->read() << 8) | _wire->read();
    ay = (_wire->read() << 8) | _wire->read();
    az = (_wire->read() << 8) | _wire->read();
    tempRaw = (_wire->read() << 8) | _wire->read();
    gx = (_wire->read() << 8) | _wire->read();
    gy = (_wire->read() << 8) | _wire->read();
    gz = (_wire->read() << 8) | _wire->read();

    return true;
}

bool MPU6050Sensor::readScaled(float &ax_m_s2, float &ay_m_s2, float &az_m_s2,
                               float &gx_rad_s, float &gy_rad_s, float &gz_rad_s,
                               float &dieTemp_C) {
    int16_t ax, ay, az, gx, gy, gz, tempRaw;
    if (!readRaw(ax, ay, az, gx, gy, gz, tempRaw)) return false;

    // Apply configurable Body Frame axis sign conventions
    const float accelScale = (9.80665f / 16384.0f);
    ax_m_s2 = ((ax * accelScale) - _calData.accel_offset_x) * BODY_AXIS_X_SIGN;
    ay_m_s2 = ((ay * accelScale) - _calData.accel_offset_y) * BODY_AXIS_Y_SIGN;
    az_m_s2 = ((az * accelScale) - _calData.accel_offset_z) * BODY_AXIS_Z_SIGN;

    const float gyroScale = ((3.14159265f / 180.0f) / 131.0f);
    gx_rad_s = ((gx * gyroScale) - _calData.gyro_offset_x) * BODY_AXIS_X_SIGN;
    gy_rad_s = ((gy * gyroScale) - _calData.gyro_offset_y) * BODY_AXIS_Y_SIGN;
    gz_rad_s = ((gz * gyroScale) - _calData.gyro_offset_z) * BODY_AXIS_Z_SIGN;

    dieTemp_C = (tempRaw / 340.0f) + 36.53f;

    return true;
}

void MPU6050Sensor::printDiagnostics() const {
    Serial.printf("  MPU6050 IMU   : Status=%s | Addr=0x%02X | Calibrated=%s\n",
                  _isInitialized ? "OK" : "FAULT", _i2cAddr, _calData.is_calibrated ? "YES" : "NO");
}

void MPU6050Sensor::writeRegister(uint8_t reg, uint8_t value) {
    _wire->beginTransmission(_i2cAddr);
    _wire->write(reg);
    _wire->write(value);
    _wire->endTransmission();
}

uint8_t MPU6050Sensor::readRegister(uint8_t reg) {
    _wire->beginTransmission(_i2cAddr);
    _wire->write(reg);
    if (_wire->endTransmission(false) != 0) return 0xFF;
    if (_wire->requestFrom(_i2cAddr, (uint8_t)1) < 1) return 0xFF;
    return _wire->read();
}
