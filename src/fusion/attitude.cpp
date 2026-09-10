#include "attitude.h"
#include <math.h>

#define RAD_TO_DEG_F (180.0f / 3.14159265f)

AttitudeFilter::AttitudeFilter(float alpha)
    : _alpha(alpha), _rollDeg(0.0f), _pitchDeg(0.0f), _isInitialized(false) {}

void AttitudeFilter::reset() {
    _rollDeg = 0.0f;
    _pitchDeg = 0.0f;
    _isInitialized = false;
}

void AttitudeFilter::update(float ax, float ay, float az,
                            float gx, float gy, float gz,
                            float dt_s, float &roll_deg, float &pitch_deg) {
    // Calculate Accelerometer Roll and Pitch angles
    float rollAccel = atan2f(ay, az) * RAD_TO_DEG_F;
    float pitchAccel = atan2f(-ax, sqrtf(ay * ay + az * az)) * RAD_TO_DEG_F;

    if (!_isInitialized) {
        _rollDeg = rollAccel;
        _pitchDeg = pitchAccel;
        _isInitialized = true;
    } else {
        // Gyro Integration + Accel Complementary Fusion
        float gx_deg_s = gx * RAD_TO_DEG_F;
        float gy_deg_s = gy * RAD_TO_DEG_F;

        _rollDeg = _alpha * (_rollDeg + gx_deg_s * dt_s) + (1.0f - _alpha) * rollAccel;
        _pitchDeg = _alpha * (_pitchDeg + gy_deg_s * dt_s) + (1.0f - _alpha) * pitchAccel;
    }

    roll_deg = _rollDeg;
    pitch_deg = _pitchDeg;
}
