#include "max31865_pt100.h"
#include <math.h>

#define MAX31865_REG_CONFIG       0x00
#define MAX31865_REG_RTD_MSB      0x01
#define MAX31865_REG_RTD_LSB      0x02
#define MAX31865_REG_FAULT_STATUS 0x07

#define MAX31865_CONFIG_VBIAS     0x80
#define MAX31865_CONFIG_AUTOSTART 0x40
#define MAX31865_CONFIG_2WIRE     0x00
#define MAX31865_CONFIG_CLEARFAULT 0x02
#define MAX31865_CONFIG_50HZ      0x01

// Callendar-Van Dusen Coefficients for PT100 (ITS-90 / DIN EN 60751)
static const float RTD_A = 3.90830e-3f;
static const float RTD_B = -5.77500e-7f;

MAX31865Sensor::MAX31865Sensor(uint8_t csPin)
    : _csPin(csPin), _spi(&SPI), _rref(MAX31865_RREF_OHMS),
      _rnominal(MAX31865_RNOMINAL_OHMS), _isInitialized(false) {}

bool MAX31865Sensor::begin(SPIClass &spiBus, float rref, float rnominal) {
    _spi = &spiBus;
    _rref = rref;
    _rnominal = rnominal;
    _isInitialized = false;

    pinMode(_csPin, OUTPUT);
    digitalWrite(_csPin, HIGH);

    // Sanity check 1: Clear faults first
    writeRegister(MAX31865_REG_CONFIG, MAX31865_CONFIG_CLEARFAULT);
    delay(5);

    // Configure MAX31865: Vbias ON, Auto Conversion Mode, 2-wire RTD, 50Hz noise rejection
    uint8_t configVal = MAX31865_CONFIG_VBIAS | MAX31865_CONFIG_AUTOSTART |
                        MAX31865_CONFIG_2WIRE | MAX31865_CONFIG_50HZ;
    writeRegister(MAX31865_REG_CONFIG, configVal);
    delay(10);

    // Sanity check 2: Read back configuration register to confirm bus communication
    uint8_t readback = readRegister(MAX31865_REG_CONFIG);
    if ((readback & 0xC0) != 0xC0) { // Check Vbias & Autostart bits
        Serial.printf("[MAX31865] ERROR: Communication sanity check failed on CS Pin %u (Readback=0x%02X, expected 0xC1)\n",
                      _csPin, readback);
        return false;
    }

    _isInitialized = true;
    Serial.printf("[MAX31865] SUCCESS: RTD Converter Initialized on SPI CS Pin %u (Rref=%.1f ohm, UNVERIFIED BENCH_TBD)\n",
                  _csPin, _rref);
    return true;
}

bool MAX31865Sensor::readRaw(uint16_t &rtdRaw, uint8_t &faultStatus) {
    if (!_isInitialized) return false;

    _spi->beginTransaction(SPISettings(MAX31865_SPI_FREQ, MSBFIRST, MAX31865_SPI_MODE));
    digitalWrite(_csPin, LOW);
    _spi->transfer(MAX31865_REG_RTD_MSB); // Read starting from RTD MSB register
    uint8_t msb = _spi->transfer(0x00);
    uint8_t lsb = _spi->transfer(0x00);
    digitalWrite(_csPin, HIGH);
    _spi->endTransaction();

    rtdRaw = ((msb << 8) | lsb) >> 1; // 15-bit RTD count (bit 0 is fault bit)

    faultStatus = readRegister(MAX31865_REG_FAULT_STATUS);
    if (faultStatus != 0) {
        // Clear fault status register
        writeRegister(MAX31865_REG_CONFIG, readRegister(MAX31865_REG_CONFIG) | MAX31865_CONFIG_CLEARFAULT);
    }

    return true;
}

bool MAX31865Sensor::readScaled(float &temperature_C, float &rtdResistance_Ohms, uint8_t &faultStatus) {
    uint16_t rtdRaw;
    if (!readRaw(rtdRaw, faultStatus)) return false;

    // Calculate measured RTD Resistance: R_rtd = (ADC_code * R_ref) / 32768.0
    rtdResistance_Ohms = ((float)rtdRaw * _rref) / 32768.0f;

    // Full Quadratic Callendar-Van Dusen Equation for T >= 0 °C:
    // R(T) = R0 * (1 + A*T + B*T^2)
    // Solving quadratic: B*T^2 + A*T + (1 - R/R0) = 0
    // T = (-A + sqrt(A^2 - 4*B*(1 - R/R0))) / (2*B)
    float ratio = rtdResistance_Ohms / _rnominal;
    float discriminant = (RTD_A * RTD_A) - (4.0f * RTD_B * (1.0f - ratio));

    if (discriminant >= 0.0f) {
        temperature_C = (-RTD_A + sqrtf(discriminant)) / (2.0f * RTD_B);
    } else {
        // Linear fallback if out of quadratic domain
        temperature_C = (rtdResistance_Ohms - _rnominal) / (_rnominal * RTD_A);
    }

    return (faultStatus == 0);
}

void MAX31865Sensor::printDiagnostics() const {
    Serial.printf("  MAX31865 RTD : Status=%s | CS Pin=%u | Rref=%.1f ohm (UNVERIFIED BENCH_TBD)\n",
                  _isInitialized ? "OK" : "FAULT", _csPin, _rref);
}

void MAX31865Sensor::writeRegister(uint8_t reg, uint8_t value) {
    _spi->beginTransaction(SPISettings(MAX31865_SPI_FREQ, MSBFIRST, MAX31865_SPI_MODE));
    digitalWrite(_csPin, LOW);
    _spi->transfer(reg | 0x80); // Write bit MSB = 1
    _spi->transfer(value);
    digitalWrite(_csPin, HIGH);
    _spi->endTransaction();
}

uint8_t MAX31865Sensor::readRegister(uint8_t reg) {
    _spi->beginTransaction(SPISettings(MAX31865_SPI_FREQ, MSBFIRST, MAX31865_SPI_MODE));
    digitalWrite(_csPin, LOW);
    _spi->transfer(reg & 0x7F); // Read bit MSB = 0
    uint8_t val = _spi->transfer(0x00);
    digitalWrite(_csPin, HIGH);
    _spi->endTransaction();
    return val;
}
