#ifndef HEADING_H
#define HEADING_H

#include <Arduino.h>
#include "config.h"

class HeadingCalculator {
public:
    HeadingCalculator();

    void setCalibration(float offsetX, float offsetY, float offsetZ,
                        float scaleX = 1.0f, float scaleY = 1.0f, float scaleZ = 1.0f);
    void setCalibration(const MagCalibrationData &cal);
    const MagCalibrationData& getCalibration() const { return _calData; }

    float calculateHeading(float magX_uT, float magY_uT, float magZ_uT,
                           float rollDeg, float pitchDeg);

    void getCalibratedMag(float magX_in, float magY_in, float magZ_in,
                          float &magX_out, float &magY_out, float &magZ_out) const;

private:
    MagCalibrationData _calData;
};

#endif // HEADING_H
