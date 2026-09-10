#ifndef PI_DETECTOR_H
#define PI_DETECTOR_H

#include <Arduino.h>
#include "config.h"

class PIDetector {
public:
    PIDetector(uint8_t gatePin = PI_MOSFET_GATE_PIN, uint8_t adcPin = PI_ADC_INPUT_PIN);

    bool begin();
    
    // Parameter configuration
    void setTiming(uint32_t width_us, uint32_t settle_us, uint32_t period_us);
    void setThreshold(float thresholdCounts);
    void setCalibration(const PiCalibrationData &cal);
    const PiCalibrationData& getCalibration() const { return _calData; }
    void resetBaseline();

    // Pulse Induction Sampling & Signal Processing
    // Executes an averaged pulse burst sequence, extracts raw decay curve, and calculates metrics
    bool processPulseSequence(uint16_t *rawDecayOut,
                              float &baselineOut,
                              float &peakOut,
                              float &earlyDecayOut,
                              float &lateDecayOut,
                              float &anomalyStrengthOut,
                              bool &isDetectedOut);

    // Executes a multi-sequence no-target calibration to estimate baseline & noise sigma
    bool calibrateNoTarget(uint16_t sequenceCount, PiCalibrationData &calOut);

    // Executes a single diagnostic pulse and returns raw decay samples
    uint16_t sampleSinglePulse(uint16_t *decayBuffer, size_t maxSamples);
    
    // Getters
    uint32_t getPulseWidthUs() const { return _pulseWidthUs; }
    uint32_t getSettlingDelayUs() const { return _settlingDelayUs; }
    uint32_t getPulsePeriodUs() const { return _pulsePeriodUs; }
    float getThreshold() const { return _detectionThreshold; }
    float getBaseline() const { return _decayBaseline; }
    bool isHealthy() const { return _isInitialized; }

    void printDiagnostics() const;

private:
    uint8_t _gatePin;
    uint8_t _adcPin;
    uint32_t _pulseWidthUs;
    uint32_t _settlingDelayUs;
    uint32_t _pulsePeriodUs;
    float _detectionThreshold;
    float _decayBaseline;
    bool _isInitialized;
    PiCalibrationData _calData;
};

#endif // PI_DETECTOR_H
