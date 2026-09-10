#ifndef MAX31865_PT100_H
#define MAX31865_PT100_H

#include <Arduino.h>
#include <SPI.h>
#include "config.h"

class MAX31865Sensor {
public:
    MAX31865Sensor(uint8_t csPin = MAX31865_CS_PIN);

    bool begin(SPIClass &spiBus = SPI, float rref = MAX31865_RREF_OHMS, float rnominal = MAX31865_RNOMINAL_OHMS);
    bool readRaw(uint16_t &rtdRaw, uint8_t &faultStatus);
    bool readScaled(float &temperature_C, float &rtdResistance_Ohms, uint8_t &faultStatus);

    bool isHealthy() const { return _isInitialized; }
    uint8_t getCsPin() const { return _csPin; }
    float getRref() const { return _rref; }
    void printDiagnostics() const;

private:
    uint8_t _csPin;
    SPIClass *_spi;
    float _rref;
    float _rnominal;
    bool _isInitialized;

    void writeRegister(uint8_t reg, uint8_t value);
    uint8_t readRegister(uint8_t reg);
};

#endif // MAX31865_PT100_H
