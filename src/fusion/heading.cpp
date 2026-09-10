#include "heading.h"
#include <math.h>

#define DEG_TO_RAD_F (3.14159265f / 180.0f)
#define RAD_TO_DEG_F (180.0f / 3.14159265f)

HeadingCalculator::HeadingCalculator() {
    _calData = {0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, false};
}

void HeadingCalculator::setCalibration(float offsetX, float offsetY, float offsetZ,
                                       float scaleX, float scaleY, float scaleZ) {
    _calData.offset_x = offsetX;
    _calData.offset_y = offsetY;
    _calData.offset_z = offsetZ;
    _calData.scale_x  = scaleX;
    _calData.scale_y  = scaleY;
    _calData.scale_z  = scaleZ;
    _calData.is_calibrated = true;
}

void HeadingCalculator::setCalibration(const MagCalibrationData &cal) {
    _calData = cal;
}

void HeadingCalculator::getCalibratedMag(float magX_in, float magY_in, float magZ_in,
                                         float &magX_out, float &magY_out, float &magZ_out) const {
    magX_out = (magX_in - _calData.offset_x) * _calData.scale_x;
    magY_out = (magY_in - _calData.offset_y) * _calData.scale_y;
    magZ_out = (magZ_in - _calData.offset_z) * _calData.scale_z;
}

float HeadingCalculator::calculateHeading(float magX_uT, float magY_uT, float magZ_uT,
                                           float rollDeg, float pitchDeg) {
    float mx, my, mz;
    getCalibratedMag(magX_uT, magY_uT, magZ_uT, mx, my, mz);

    float phi = rollDeg * DEG_TO_RAD_F;
    float theta = pitchDeg * DEG_TO_RAD_F;

    // Tilt compensation (project 3D magnetic vector into horizontal plane)
    float Xh = mx * cosf(theta) + my * sinf(phi) * sinf(theta) + mz * cosf(phi) * sinf(theta);
    float Yh = my * cosf(phi) - mz * sinf(phi);

    float headingRad = atan2f(-Yh, Xh);
    float headingDeg = headingRad * RAD_TO_DEG_F;

    if (headingDeg < 0.0f) headingDeg += 360.0f;
    if (headingDeg >= 360.0f) headingDeg -= 360.0f;

    return headingDeg;
}
