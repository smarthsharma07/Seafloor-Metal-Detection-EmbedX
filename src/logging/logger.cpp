#include "logger.h"

TelemetryLogger::TelemetryLogger()
    : _isInitialized(false), _logFilePath("/telemetry.csv") {}

bool TelemetryLogger::begin() {
    _isInitialized = false;

    if (!LittleFS.begin(true)) { // Mount LittleFS (format on failure)
        Serial.println("[LOGGER] ERROR: Failed to mount LittleFS filesystem");
        return false;
    }

    _isInitialized = true;
    ensureHeaderExists();
    Serial.printf("[LOGGER] SUCCESS: Storage Logger Initialized (File: %s, Size: %u bytes)\n",
                  _logFilePath.c_str(), getLogFileSize());
    return true;
}

void TelemetryLogger::ensureHeaderExists() {
    if (!_isInitialized) return;

    if (!LittleFS.exists(_logFilePath)) {
        File file = LittleFS.open(_logFilePath, FILE_WRITE);
        if (file) {
            file.println("timestamp,valid_mpu,valid_mag,valid_rtd,valid_rtc,valid_pi,valid_imu_dir,"
                         "water_temp_C,imu_temp_C,rtd_ohms,accel_x,accel_y,accel_z,gyro_x,gyro_y,gyro_z,"
                         "mag_x,mag_y,mag_z,heading,roll,pitch,pi_baseline,pi_peak,pi_early,pi_late,"
                         "pi_strength,pi_detected,accel_horiz_mag,imu_heading,drift_bearing,drift_radius_m,"
                         "uncertainty_deg,mag_conf,imu_conf,fusion_conf");
            file.close();
            Serial.println("[LOGGER] Created new telemetry.csv file with complete telemetry schema");
        }
    }
}

bool TelemetryLogger::logSample(const SensorSample &sample) {
    if (!_isInitialized) return false;

    File file = LittleFS.open(_logFilePath, FILE_APPEND);
    if (!file) return false;

    char line[512];
    snprintf(line, sizeof(line),
             "%s,%d,%d,%d,%d,%d,%d,"
             "%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.3f,%.3f,%.3f,"
             "%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.1f,%.1f,%.1f,%.1f,"
             "%.1f,%d,%.2f,%.1f,%.2f,%.1f,"
             "%.1f,%.2f,%.2f,%.2f\n",
             sample.iso_timestamp,
             sample.mpu_valid ? 1 : 0,
             sample.mag_valid ? 1 : 0,
             sample.rtd_valid ? 1 : 0,
             sample.rtc_valid ? 1 : 0,
             sample.pi_valid ? 1 : 0,
             sample.imu_direction_valid ? 1 : 0,
             sample.water_temperature_C,
             sample.imu_die_temperature_C,
             sample.rtd_resistance_ohms,
             sample.accel_x_m_s2, sample.accel_y_m_s2, sample.accel_z_m_s2,
             sample.gyro_x_rad_s, sample.gyro_y_rad_s, sample.gyro_z_rad_s,
             sample.mag_x_uT, sample.mag_y_uT, sample.mag_z_uT,
             sample.magnetic_heading_deg,
             sample.roll_deg, sample.pitch_deg,
             sample.pi_baseline, sample.pi_peak, sample.pi_early_decay, sample.pi_late_decay,
             sample.pi_anomaly_strength, sample.pi_detected ? 1 : 0,
             sample.accel_horiz_mag, sample.imu_drift_heading_deg,
             sample.estimated_drift_bearing_deg, sample.deployment_drift_radius_m,
             sample.uncertainty_angle_deg,
             sample.magnetic_confidence, sample.imu_confidence, sample.fusion_confidence);

    size_t written = file.print(line);
    file.close();
    return (written > 0);
}

void TelemetryLogger::flush() {
    // LittleFS closes files after every write; no lingering volatile buffer
}

size_t TelemetryLogger::getLogFileSize() const {
    if (!_isInitialized || !LittleFS.exists(_logFilePath)) return 0;
    File file = LittleFS.open(_logFilePath, FILE_READ);
    if (!file) return 0;
    size_t size = file.size();
    file.close();
    return size;
}

void TelemetryLogger::printDiagnostics() const {
    Serial.printf("  Data Logger  : Status=%s | File=%s | Size=%u bytes (LittleFS CSV)\n",
                  _isInitialized ? "OK" : "FAULT", _logFilePath.c_str(), getLogFileSize());
}
