#ifndef RM3100_H
#define RM3100_H

#include <Arduino.h>
#include <SPI.h>
#include "config.h"

class RM3100Sensor {
public:
    RM3100Sensor(uint8_t csPin = RM3100_CS_PIN);

    bool begin(SPIClass &spiBus = SPI);
    bool readRaw(int32_t &magX, int32_t &magY, int32_t &magZ);
    bool readScaled(float &magX_uT, float &magY_uT, float &magZ_uT);

    bool isHealthy() const { return _isInitialized; }
    uint8_t getCsPin() const { return _csPin; }
    void printDiagnostics() const;

private:
    uint8_t _csPin;
    SPIClass *_spi;
    bool _isInitialized;
    float _gainLSBperUT; // Count to uT conversion scale

    void writeRegister(uint8_t reg, uint8_t value);
    uint8_t readRegister(uint8_t reg);
};

#endif // RM3100_H
