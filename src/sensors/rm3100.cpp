#include "rm3100.h"

#define RM3100_REG_POLL    0x00
#define RM3100_REG_CMM     0x01
#define RM3100_REG_CCX_MSB 0x04
#define RM3100_REG_CCX_LSB 0x05
#define RM3100_REG_CCY_MSB 0x06
#define RM3100_REG_CCY_LSB 0x07
#define RM3100_REG_CCZ_MSB 0x08
#define RM3100_REG_CCZ_LSB 0x09
#define RM3100_REG_STATUS  0x34
#define RM3100_REG_MX2     0x24
#define RM3100_REG_REVID   0x36

RM3100Sensor::RM3100Sensor(uint8_t csPin)
    : _csPin(csPin), _spi(&SPI), _isInitialized(false), _gainLSBperUT(75.0f) {}

bool RM3100Sensor::begin(SPIClass &spiBus) {
    _spi = &spiBus;
    _isInitialized = false;

    pinMode(_csPin, OUTPUT);
    digitalWrite(_csPin, HIGH);

    // Sanity check 1: Read Revision ID (REVID register 0x36 should return valid non-zero/non-0xFF ID)
    uint8_t revId = readRegister(RM3100_REG_REVID);
    if (revId == 0x00 || revId == 0xFF) {
        Serial.printf("[RM3100] ERROR: Communication sanity check failed on SPI CS Pin %u (REVID=0x%02X)\n", _csPin, revId);
        return false;
    }

    // Set Cycle Count Registers for X, Y, Z to 200 counts (0x00C8) -> 75 LSB/uT gain
    writeRegister(RM3100_REG_CCX_MSB, 0x00);
    writeRegister(RM3100_REG_CCX_LSB, 0xC8);
    writeRegister(RM3100_REG_CCY_MSB, 0x00);
    writeRegister(RM3100_REG_CCY_LSB, 0xC8);
    writeRegister(RM3100_REG_CCZ_MSB, 0x00);
    writeRegister(RM3100_REG_CCZ_LSB, 0xC8);

    // Sanity check 2: Verify Cycle Count readback
    uint8_t lsbX = readRegister(RM3100_REG_CCX_LSB);
    if (lsbX != 0xC8) {
        Serial.printf("[RM3100] WARNING: Cycle count readback mismatch (0x%02X != 0xC8). Gain may vary.\n", lsbX);
    }

    // Enable Continuous Measurement Mode (CMM: START bit = 1, X/Y/Z enabled = 0x79)
    writeRegister(RM3100_REG_CMM, 0x79);
    delay(10);

    _isInitialized = true;
    Serial.printf("[RM3100] SUCCESS: Magnetometer Initialized and Verified on SPI CS Pin %u (REVID=0x%02X, Gain=75 LSB/uT)\n", _csPin, revId);
    return true;
}

bool RM3100Sensor::readRaw(int32_t &magX, int32_t &magY, int32_t &magZ) {
    if (!_isInitialized) return false;

    // Check status register (bit 7 = DRDY)
    uint8_t status = readRegister(RM3100_REG_STATUS);
    if ((status & 0x80) == 0) {
        // Data not ready yet
        return false;
    }

    _spi->beginTransaction(SPISettings(RM3100_SPI_FREQ, MSBFIRST, RM3100_SPI_MODE));
    digitalWrite(_csPin, LOW);
    _spi->transfer(RM3100_REG_MX2 | 0x80); // Read starting from MX2 (read bit MSB = 1)
    
    // RM3100 uses 24-bit 2's complement signed integers per axis (9 bytes total)
    uint8_t x2 = _spi->transfer(0x00);
    uint8_t x1 = _spi->transfer(0x00);
    uint8_t x0 = _spi->transfer(0x00);

    uint8_t y2 = _spi->transfer(0x00);
    uint8_t y1 = _spi->transfer(0x00);
    uint8_t y0 = _spi->transfer(0x00);

    uint8_t z2 = _spi->transfer(0x00);
    uint8_t z1 = _spi->transfer(0x00);
    uint8_t z0 = _spi->transfer(0x00);

    digitalWrite(_csPin, HIGH);
    _spi->endTransaction();

    // Sign extend 24-bit integers to 32-bit signed integers
    magX = (int32_t)((x2 << 16) | (x1 << 8) | x0);
    if (x2 & 0x80) magX |= 0xFF000000;

    magY = (int32_t)((y2 << 16) | (y1 << 8) | y0);
    if (y2 & 0x80) magY |= 0xFF000000;

    magZ = (int32_t)((z2 << 16) | (z1 << 8) | z0);
    if (z2 & 0x80) magZ |= 0xFF000000;

    return true;
}

bool RM3100Sensor::readScaled(float &magX_uT, float &magY_uT, float &magZ_uT) {
    int32_t rawX, rawY, rawZ;
    if (!readRaw(rawX, rawY, rawZ)) return false;

    // Convert raw counts to microtesla (uT): 1 uT = 75 LSB (at 200 cycle count)
    // Apply configurable Body Frame axis sign conventions
    magX_uT = ((float)rawX / _gainLSBperUT) * BODY_AXIS_X_SIGN;
    magY_uT = ((float)rawY / _gainLSBperUT) * BODY_AXIS_Y_SIGN;
    magZ_uT = ((float)rawZ / _gainLSBperUT) * BODY_AXIS_Z_SIGN;

    return true;
}

void RM3100Sensor::printDiagnostics() const {
    Serial.printf("  RM3100 Mag   : Status=%s | CS Pin=%u | Gain=75 LSB/uT (200 CC)\n",
                  _isInitialized ? "OK" : "FAULT", _csPin);
}

void RM3100Sensor::writeRegister(uint8_t reg, uint8_t value) {
    _spi->beginTransaction(SPISettings(RM3100_SPI_FREQ, MSBFIRST, RM3100_SPI_MODE));
    digitalWrite(_csPin, LOW);
    _spi->transfer(reg & 0x7F); // Write bit MSB = 0
    _spi->transfer(value);
    digitalWrite(_csPin, HIGH);
    _spi->endTransaction();
}

uint8_t RM3100Sensor::readRegister(uint8_t reg) {
    _spi->beginTransaction(SPISettings(RM3100_SPI_FREQ, MSBFIRST, RM3100_SPI_MODE));
    digitalWrite(_csPin, LOW);
    _spi->transfer(reg | 0x80); // Read bit MSB = 1
    uint8_t val = _spi->transfer(0x00);
    digitalWrite(_csPin, HIGH);
    _spi->endTransaction();
    return val;
}
