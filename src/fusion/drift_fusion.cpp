#include "drift_fusion.h"
#include <math.h>

#define DEG_TO_RAD_F (3.14159265f / 180.0f)
#define RAD_TO_DEG_F (180.0f / 3.14159265f)
#define GRAVITY_M_S2 9.80665f

DriftFusionEngine::DriftFusionEngine(float wMag, float wImu)
    : _weightMag(wMag), _weightImu(wImu),
      _deploymentDriftRadiusM(DEFAULT_DRIFT_RADIUS_M),
      _uncertaintyDeg(DEFAULT_UNCERTAINTY_DEG) {}

void DriftFusionEngine::setWeights(float wMag, float wImu) {
    _weightMag = wMag;
    _weightImu = wImu;
}

void DriftFusionEngine::setDriftParams(float radius_m, float uncertainty_deg) {
    _deploymentDriftRadiusM = radius_m;
    _uncertaintyDeg = uncertainty_deg;
}

void DriftFusionEngine::computeDriftSector(float magHeading_deg, float magFieldMagnitude_uT,
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
                                            float &fusionConfidence) {
    // 1. Evaluate Magnetic Heading & Field Confidence
    if (!magValid) {
        magneticConfidence = 0.0f;
    } else {
        // Nominal Earth field magnitude is ~30-60 uT
        if (magFieldMagnitude_uT >= 20.0f && magFieldMagnitude_uT <= 70.0f) {
            magneticConfidence = 0.95f;
        } else if (magFieldMagnitude_uT > 5.0f && magFieldMagnitude_uT < 100.0f) {
            magneticConfidence = 0.60f; // Weak or disturbed field
        } else {
            magneticConfidence = 0.10f; // Severe magnetic anomaly or saturation
        }
    }

    // 2. Remove Gravity Vector from Accelerometer to extract horizontal motion
    float phi = rollDeg * DEG_TO_RAD_F;
    float theta = pitchDeg * DEG_TO_RAD_F;
    float psi = magHeading_deg * DEG_TO_RAD_F;

    float gx = -GRAVITY_M_S2 * sinf(theta);
    float gy =  GRAVITY_M_S2 * sinf(phi) * cosf(theta);
    float gz =  GRAVITY_M_S2 * cosf(phi) * cosf(theta);

    float ax_dyn = accelX_m_s2 - gx;
    float ay_dyn = accelY_m_s2 - gy;
    float az_dyn = accelZ_m_s2 - gz;

    // Transform dynamic acceleration into geographic North / East coordinates
    float aN = ax_dyn * cosf(theta) * cosf(psi) +
               ay_dyn * (sinf(phi) * sinf(theta) * cosf(psi) - cosf(phi) * sinf(psi)) +
               az_dyn * (cosf(phi) * sinf(theta) * cosf(psi) + sinf(phi) * sinf(psi));

    float aE = ax_dyn * cosf(theta) * sinf(psi) +
               ay_dyn * (sinf(phi) * sinf(theta) * sinf(psi) + cosf(phi) * cosf(psi)) +
               az_dyn * (cosf(phi) * sinf(theta) * sinf(psi) - sinf(phi) * cosf(psi));

    accelHorizMag_m_s2 = sqrtf(aN * aN + aE * aE);

    // 3. Conditional IMU Motion Evaluation
    if (mpuValid && (accelHorizMag_m_s2 >= IMU_MOTION_THRESHOLD_M_S2)) {
        imuDirectionValid = true;
        float imuRad = atan2f(aE, aN);
        float imuDeg = imuRad * RAD_TO_DEG_F;
        if (imuDeg < 0.0f) imuDeg += 360.0f;
        imuHeading_deg = imuDeg;

        // IMU Confidence scales with horizontal acceleration signal-to-noise
        imuConfidence = accelHorizMag_m_s2 / 1.5f;
        if (imuConfidence > 1.0f) imuConfidence = 1.0f;
        if (imuConfidence < 0.1f) imuConfidence = 0.1f;
    } else {
        imuDirectionValid = false;
        imuHeading_deg = 0.0f;
        imuConfidence = 0.0f;
    }

    // 4. Directional Fusion
    if (imuDirectionValid && magValid) {
        // Weighted vector sum of fin-aligned magnetic heading and transient IMU direction
        float sinSum = (_weightMag * sinf(psi)) + (_weightImu * sinf(imuHeading_deg * DEG_TO_RAD_F));
        float cosSum = (_weightMag * cosf(psi)) + (_weightImu * cosf(imuHeading_deg * DEG_TO_RAD_F));

        float fusedRad = atan2f(sinSum, cosSum);
        float fusedDeg = fusedRad * RAD_TO_DEG_F;
        if (fusedDeg < 0.0f) fusedDeg += 360.0f;
        estimatedDriftBearing_deg = fusedDeg;

        // Angular agreement metric
        float angularDiff = fabsf(magHeading_deg - imuHeading_deg);
        if (angularDiff > 180.0f) angularDiff = 360.0f - angularDiff;
        float agreement = cosf(angularDiff * DEG_TO_RAD_F * 0.5f);

        fusionConfidence = ((0.75f * magneticConfidence) + (0.25f * imuConfidence)) * agreement;
    } else if (magValid) {
        // Rely 100% on passive current-alignment fins via magnetic heading proxy
        estimatedDriftBearing_deg = magHeading_deg;
        fusionConfidence = magneticConfidence * 0.85f;
    } else {
        // Fault fallback
        estimatedDriftBearing_deg = 0.0f;
        fusionConfidence = 0.0f;
    }

    if (fusionConfidence < 0.05f && (magValid || mpuValid)) {
        fusionConfidence = 0.05f;
    }

    deploymentDriftRadius_m = _deploymentDriftRadiusM;
    uncertaintyAngle_deg = _uncertaintyDeg;
}
