#ifndef ATTITUDE_H
#define ATTITUDE_H

#include <Arduino.h>
#include "config.h"

class AttitudeFilter {
public:
    AttitudeFilter(float alpha = ATTITUDE_ALPHA);

    void reset();
    void update(float ax_m_s2, float ay_m_s2, float az_m_s2,
                float gx_rad_s, float gy_rad_s, float gz_rad_s,
                float dt_s, float &roll_deg, float &pitch_deg);

    float getRollDeg() const { return _rollDeg; }
    float getPitchDeg() const { return _pitchDeg; }

private:
    float _alpha;
    float _rollDeg;
    float _pitchDeg;
    bool _isInitialized;
};

#endif // ATTITUDE_H
