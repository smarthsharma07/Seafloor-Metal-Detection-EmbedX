#include "pi_detector.h"
#include <math.h>

PIDetector::PIDetector(uint8_t gatePin, uint8_t adcPin)
    : _gatePin(gatePin), _adcPin(adcPin),
      _pulseWidthUs(PI_PULSE_WIDTH_US),
      _settlingDelayUs(PI_SETTLING_DELAY_US),
      _pulsePeriodUs(PI_PULSE_PERIOD_US),
      _detectionThreshold(PI_DEFAULT_THRESHOLD),
      _decayBaseline(0.0f),
      _isInitialized(false) {
    _calData = {0.0f, 0.0f, PI_DEFAULT_THRESHOLD, false};
}

bool PIDetector::begin() {
    pinMode(_gatePin, OUTPUT);
    digitalWrite(_gatePin, LOW);

    // Configure ADC1 input pin (safe during Wi-Fi operations)
    analogReadResolution(12); // 12-bit ADC (0 to 4095)
    analogSetAttenuation(ADC_11db); // Full-scale 0 - 3.3V range

    _decayBaseline = 0.0f;
    _isInitialized = true;

    Serial.printf("[PI_DETECTOR] SUCCESS: Pulse Induction Engine Initialized (Gate=GPIO%u, ADC=GPIO%u)\n",
                  _gatePin, _adcPin);
    Serial.printf("               PROVISIONAL TIMINGS: Width=%uus, Settle=%uus, Period=%uus\n",
                  _pulseWidthUs, _settlingDelayUs, _pulsePeriodUs);
    return true;
}

void PIDetector::setTiming(uint32_t width_us, uint32_t settle_us, uint32_t period_us) {
    _pulseWidthUs = width_us;
    _settlingDelayUs = settle_us;
    _pulsePeriodUs = period_us;
    Serial.printf("[PI_DETECTOR] Updated Timing: Width=%uus, Settle=%uus, Period=%uus\n",
                  _pulseWidthUs, _settlingDelayUs, _pulsePeriodUs);
}

void PIDetector::setThreshold(float thresholdCounts) {
    _detectionThreshold = thresholdCounts;
    Serial.printf("[PI_DETECTOR] Updated Threshold: %.1f ADC counts\n", _detectionThreshold);
}

void PIDetector::setCalibration(const PiCalibrationData &cal) {
    _calData = cal;
    _decayBaseline = cal.baseline_counts;
    _detectionThreshold = cal.derived_threshold;
    Serial.printf("[PI_DETECTOR] Applied Calibration: Baseline=%.1f, Sigma=%.2f, Threshold=%.1f\n",
                  _calData.baseline_counts, _calData.noise_sigma, _calData.derived_threshold);
}

void PIDetector::resetBaseline() {
    _decayBaseline = 0.0f;
    Serial.println("[PI_DETECTOR] Baseline reset to 0.0");
}

uint16_t PIDetector::sampleSinglePulse(uint16_t *decayBuffer, size_t maxSamples) {
    if (!_isInitialized) return 0;

    // 1. Energize PI coil (MOSFET ON)
    digitalWrite(_gatePin, HIGH);
    delayMicroseconds(_pulseWidthUs);

    // 2. Turn MOSFET OFF (Inductive back-EMF spike begins)
    digitalWrite(_gatePin, LOW);

    // 3. Wait for clamping diode / switching transient settling
    delayMicroseconds(_settlingDelayUs);

    // 4. Sample microsecond decay curve
    uint32_t sum = 0;
    size_t samplesTaken = (maxSamples < PI_SAMPLE_COUNT) ? maxSamples : PI_SAMPLE_COUNT;
    for (size_t s = 0; s < samplesTaken; s++) {
        uint16_t raw = analogRead(_adcPin);
        if (decayBuffer != nullptr) {
            decayBuffer[s] = raw;
        }
        sum += raw;
        delayMicroseconds(2);
    }

    return (uint16_t)(sum / (samplesTaken > 0 ? samplesTaken : 1));
}

bool PIDetector::processPulseSequence(uint16_t *rawDecayOut,
                                      float &baselineOut,
                                      float &peakOut,
                                      float &earlyDecayOut,
                                      float &lateDecayOut,
                                      float &anomalyStrengthOut,
                                      bool &isDetectedOut) {
    if (!_isInitialized) return false;

    uint32_t decayAccumulator[PI_SAMPLE_COUNT] = {0};
    const uint8_t numPulses = PI_AVERAGING_COUNT;

    for (uint8_t p = 0; p < numPulses; p++) {
        uint32_t pulseStartMicros = micros();

        // 1. Energize coil
        digitalWrite(_gatePin, HIGH);
        delayMicroseconds(_pulseWidthUs);

        // 2. Turn OFF
        digitalWrite(_gatePin, LOW);

        // 3. Settle
        delayMicroseconds(_settlingDelayUs);

        // 4. Sample decay waveform into accumulator
        for (uint8_t s = 0; s < PI_SAMPLE_COUNT; s++) {
            decayAccumulator[s] += analogRead(_adcPin);
            delayMicroseconds(2);
        }

        // Repetition period pacing
        uint32_t elapsed = micros() - pulseStartMicros;
        if (elapsed < _pulsePeriodUs) {
            delayMicroseconds(_pulsePeriodUs - elapsed);
        }
    }

    // Compute averaged waveform and metrics
    uint16_t maxVal = 0;
    uint32_t earlySum = 0;
    uint32_t lateSum = 0;
    uint32_t totalSum = 0;

    for (uint8_t s = 0; s < PI_SAMPLE_COUNT; s++) {
        uint16_t avgSample = (uint16_t)(decayAccumulator[s] / numPulses);
        if (rawDecayOut != nullptr) {
            rawDecayOut[s] = avgSample;
        }
        if (avgSample > maxVal) maxVal = avgSample;
        totalSum += avgSample;

        if (s < 4) earlySum += avgSample;
        if (s >= (PI_SAMPLE_COUNT - 4)) lateSum += avgSample;
    }

    float meanSignal = (float)totalSum / (float)PI_SAMPLE_COUNT;
    peakOut = (float)maxVal;
    earlyDecayOut = (float)earlySum / 4.0f;
    lateDecayOut = (float)lateSum / 4.0f;

    // Initialize or update baseline (EMA)
    if (_decayBaseline <= 0.0f) {
        _decayBaseline = meanSignal;
    } else {
        _decayBaseline = (PI_BASELINE_ALPHA * meanSignal) + ((1.0f - PI_BASELINE_ALPHA) * _decayBaseline);
    }

    baselineOut = _decayBaseline;

    // Anomaly strength = peak elevation over baseline
    anomalyStrengthOut = peakOut - _decayBaseline;
    if (anomalyStrengthOut < 0.0f) anomalyStrengthOut = 0.0f;

    isDetectedOut = (anomalyStrengthOut >= _detectionThreshold);
    return true;
}

bool PIDetector::calibrateNoTarget(uint16_t sequenceCount, PiCalibrationData &calOut) {
    if (!_isInitialized || sequenceCount < 5) return false;

    float sum = 0.0f;
    float sumSq = 0.0f;

    // Allocate temporary sample array
    float *samples = new float[sequenceCount];
    if (!samples) return false;

    for (uint16_t i = 0; i < sequenceCount; i++) {
        uint16_t decay[PI_SAMPLE_COUNT];
        float base, peak, early, late, str;
        bool det;
        processPulseSequence(decay, base, peak, early, late, str, det);
        samples[i] = peak;
        sum += peak;
        delay(10);
    }

    float mean = sum / sequenceCount;
    for (uint16_t i = 0; i < sequenceCount; i++) {
        float diff = samples[i] - mean;
        sumSq += (diff * diff);
    }
    delete[] samples;

    float variance = sumSq / sequenceCount;
    float sigma = sqrtf(variance);

    calOut.baseline_counts = mean;
    calOut.noise_sigma = sigma;
    calOut.derived_threshold = (PI_THRESHOLD_SIGMA_MULT * sigma);
    if (calOut.derived_threshold < 15.0f) {
        calOut.derived_threshold = 15.0f; // Minimum noise floor safeguard
    }
    calOut.is_calibrated = true;

    setCalibration(calOut);
    return true;
}

void PIDetector::printDiagnostics() const {
    Serial.printf("  PI Detector  : Status=%s | Gate=GPIO%u | ADC=GPIO%u | Baseline=%.1f | Thresh=%.1f | Sigma=%.2f\n",
                  _isInitialized ? "OK" : "FAULT", _gatePin, _adcPin, _decayBaseline, _detectionThreshold, _calData.noise_sigma);
    Serial.printf("                 Timing: Pulse=%uus | Settle=%uus | Period=%uus (PROVISIONAL BENCH_TBD)\n",
                  _pulseWidthUs, _settlingDelayUs, _pulsePeriodUs);
}
