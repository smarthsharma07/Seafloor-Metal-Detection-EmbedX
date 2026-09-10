#ifndef DRIFT_FUSION_H
#define DRIFT_FUSION_H

#include <Arduino.h>
#include "config.h"

class DriftFusionEngine {
public:
    DriftFusionEngine(float wMag = FUSION_WEIGHT_MAG, float wImu = FUSION_WEIGHT_IMU);

    void setWeights(float wMag, float wImu);
    void setDriftParams(float radius_m, float uncertainty_deg);

    // Fuses RM3100 magnetic heading (fin proxy) and MPU6050 short-term horizontal acceleration
    void computeDriftSector(float magHeading_deg, float magFieldMagnitude_uT,
                            float accelX_m_s2, float accelY_m_s2, float accelZ_m_s2,
                            float rollDeg, float pitchDeg,
                            bool magValid, bool mpuValid,
                            float &accelHorizMag_m_s2,
                            float &imuHeading_deg,
                            bool &imuDirectionValid,
                            float &estimatedDriftBearing_deg,
                            float &uncertaintyAngle_deg,
                            float &deploymentDriftRadius_m,
                            float &magneticConfidence,
                            float &imuConfidence,
                            float &fusionConfidence);

private:
    float _weightMag;
    float _weightImu;
    float _deploymentDriftRadiusM;
    float _uncertaintyDeg;
};

#endif // DRIFT_FUSION_H
