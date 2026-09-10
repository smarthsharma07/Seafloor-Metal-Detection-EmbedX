#include "wifi_manager.h"
#include "sensors/mpu6050.h"
#include "fusion/heading.h"
#include "pi/pi_detector.h"

extern SensorSample g_latestSample;
extern MPU6050Sensor g_mpu;
extern HeadingCalculator g_heading;
extern PIDetector g_pi;

WiFiRecoveryManager::WiFiRecoveryManager()
    : _server(80), _isRunning(false) {}

bool WiFiRecoveryManager::begin(const char *ssid, const char *password) {
    WiFi.mode(WIFI_AP);
    bool apResult = WiFi.softAP(ssid, password);

    if (!apResult) {
        Serial.println("[WIFI] ERROR: Failed to start Access Point");
        return false;
    }

    IPAddress apIP = WiFi.softAPIP();
    setupRoutes();
    _server.begin();
    _isRunning = true;

    Serial.printf("[WIFI] SUCCESS: Access Point '%s' Started. IP Address: %s\n",
                  ssid, apIP.toString().c_str());
    return true;
}

void WiFiRecoveryManager::handleClient() {
    if (_isRunning) {
        _server.handleClient();
    }
}

void WiFiRecoveryManager::stop() {
    if (_isRunning) {
        _server.stop();
        WiFi.softAPdisconnect(true);
        _isRunning = false;
        Serial.println("[WIFI] Access Point Stopped");
    }
}

void WiFiRecoveryManager::setupRoutes() {
    _server.on("/", HTTP_GET, std::bind(&WiFiRecoveryManager::handleRoot, this));
    _server.on("/status", HTTP_GET, std::bind(&WiFiRecoveryManager::handleStatus, this));
    _server.on("/config", HTTP_GET, std::bind(&WiFiRecoveryManager::handleConfig, this));
    _server.on("/calibration", HTTP_GET, std::bind(&WiFiRecoveryManager::handleCalibration, this));
    _server.on("/files", HTTP_GET, std::bind(&WiFiRecoveryManager::handleFileList, this));
    _server.on("/download", HTTP_GET, std::bind(&WiFiRecoveryManager::handleDownload, this));
    _server.onNotFound(std::bind(&WiFiRecoveryManager::handleNotFound, this));
}

void WiFiRecoveryManager::handleRoot() {
    String html = "<html><head><title>MoES Recovery Dashboard</title></head><body>";
    html += "<h1>MoES Seafloor Sensor Recovery Mode</h1>";
    html += "<p><b>IP Address:</b> " + WiFi.softAPIP().toString() + "</p>";
    html += "<ul>";
    html += "<li><a href='/status'>/status - Live JSON Telemetry & Confidences</a></li>";
    html += "<li><a href='/config'>/config - Active Parameters & Pin Mapping</a></li>";
    html += "<li><a href='/calibration'>/calibration - Sensor Offsets & Noise Calibration</a></li>";
    html += "<li><a href='/files'>/files - List Telemetry Files</a></li>";
    html += "<li><a href='/download?file=/telemetry.csv'>/download?file=/telemetry.csv - Download Log Data</a></li>";
    html += "</ul></body></html>";
    _server.send(200, "text/html", html);
}

void WiFiRecoveryManager::handleStatus() {
    char json[768];
    snprintf(json, sizeof(json),
             "{\"timestamp\":\"%s\","
             "\"validity\":{\"mpu\":%s,\"mag\":%s,\"rtd\":%s,\"rtc\":%s,\"pi\":%s,\"imu_dir\":%s},"
             "\"environmental\":{\"water_temp_C\":%.2f,\"rtd_ohms\":%.2f,\"imu_die_temp_C\":%.2f},"
             "\"motion\":{\"accel\":{\"x\":%.2f,\"y\":%.2f,\"z\":%.2f},\"gyro\":{\"x\":%.3f,\"y\":%.3f,\"z\":%.3f},"
             "\"roll_deg\":%.2f,\"pitch_deg\":%.2f,\"accel_horiz_mag\":%.2f},"
             "\"magnetic\":{\"field_uT\":{\"x\":%.2f,\"y\":%.2f,\"z\":%.2f},\"heading_deg\":%.2f,\"confidence\":%.2f},"
             "\"pi\":{\"baseline\":%.1f,\"peak\":%.1f,\"early_decay\":%.1f,\"late_decay\":%.1f,\"strength\":%.1f,\"detected\":%s},"
             "\"drift_fusion\":{\"bearing_deg\":%.2f,\"radius_m\":%.1f,\"uncertainty_deg\":%.1f,\"imu_confidence\":%.2f,\"fusion_confidence\":%.2f}}",
             g_latestSample.iso_timestamp,
             g_latestSample.mpu_valid ? "true" : "false",
             g_latestSample.mag_valid ? "true" : "false",
             g_latestSample.rtd_valid ? "true" : "false",
             g_latestSample.rtc_valid ? "true" : "false",
             g_latestSample.pi_valid ? "true" : "false",
             g_latestSample.imu_direction_valid ? "true" : "false",
             g_latestSample.water_temperature_C, g_latestSample.rtd_resistance_ohms, g_latestSample.imu_die_temperature_C,
             g_latestSample.accel_x_m_s2, g_latestSample.accel_y_m_s2, g_latestSample.accel_z_m_s2,
             g_latestSample.gyro_x_rad_s, g_latestSample.gyro_y_rad_s, g_latestSample.gyro_z_rad_s,
             g_latestSample.roll_deg, g_latestSample.pitch_deg, g_latestSample.accel_horiz_mag,
             g_latestSample.mag_x_uT, g_latestSample.mag_y_uT, g_latestSample.mag_z_uT,
             g_latestSample.magnetic_heading_deg, g_latestSample.magnetic_confidence,
             g_latestSample.pi_baseline, g_latestSample.pi_peak, g_latestSample.pi_early_decay, g_latestSample.pi_late_decay,
             g_latestSample.pi_anomaly_strength, g_latestSample.pi_detected ? "true" : "false",
             g_latestSample.estimated_drift_bearing_deg, g_latestSample.deployment_drift_radius_m,
             g_latestSample.uncertainty_angle_deg, g_latestSample.imu_confidence, g_latestSample.fusion_confidence);

    _server.send(200, "application/json", json);
}

void WiFiRecoveryManager::handleConfig() {
    char json[512];
    snprintf(json, sizeof(json),
             "{\"pins\":{\"sda\":%u,\"scl\":%u,\"miso\":%u,\"mosi\":%u,\"sck\":%u,\"rm_cs\":%u,\"rtd_cs\":%u,\"pi_gate\":%u,\"pi_adc\":%u},"
             "\"pi\":{\"width_us\":%u,\"settle_us\":%u,\"period_us\":%u,\"threshold\":%.1f},"
             "\"fusion\":{\"w_mag\":%.2f,\"w_imu\":%.2f,\"drift_radius_m\":%.1f,\"uncertainty_deg\":%.1f,\"motion_thresh_m_s2\":%.2f}}",
             I2C_SDA_PIN, I2C_SCL_PIN, SPI_MISO_PIN, SPI_MOSI_PIN, SPI_SCK_PIN, RM3100_CS_PIN, MAX31865_CS_PIN, PI_MOSFET_GATE_PIN, PI_ADC_INPUT_PIN,
             PI_PULSE_WIDTH_US, PI_SETTLING_DELAY_US, PI_PULSE_PERIOD_US, PI_DEFAULT_THRESHOLD,
             FUSION_WEIGHT_MAG, FUSION_WEIGHT_IMU, DEFAULT_DRIFT_RADIUS_M, DEFAULT_UNCERTAINTY_DEG, IMU_MOTION_THRESHOLD_M_S2);
    _server.send(200, "application/json", json);
}

void WiFiRecoveryManager::handleCalibration() {
    const MagCalibrationData &magCal = g_heading.getCalibration();
    const ImuCalibrationData &imuCal = g_mpu.getCalibration();
    const PiCalibrationData &piCal = g_pi.getCalibration();

    char json[600];
    snprintf(json, sizeof(json),
             "{\"mag\":{\"offset\":{\"x\":%.2f,\"y\":%.2f,\"z\":%.2f},\"scale\":{\"x\":%.2f,\"y\":%.2f,\"z\":%.2f},\"calibrated\":%s},"
             "\"imu\":{\"accel_offset\":{\"x\":%.3f,\"y\":%.3f,\"z\":%.3f},\"gyro_offset\":{\"x\":%.4f,\"y\":%.4f,\"z\":%.4f},\"calibrated\":%s},"
             "\"pi\":{\"baseline\":%.1f,\"noise_sigma\":%.2f,\"derived_threshold\":%.1f,\"calibrated\":%s}}",
             magCal.offset_x, magCal.offset_y, magCal.offset_z,
             magCal.scale_x, magCal.scale_y, magCal.scale_z,
             magCal.is_calibrated ? "true" : "false",
             imuCal.accel_offset_x, imuCal.accel_offset_y, imuCal.accel_offset_z,
             imuCal.gyro_offset_x, imuCal.gyro_offset_y, imuCal.gyro_offset_z,
             imuCal.is_calibrated ? "true" : "false",
             piCal.baseline_counts, piCal.noise_sigma, piCal.derived_threshold,
             piCal.is_calibrated ? "true" : "false");
    _server.send(200, "application/json", json);
}

void WiFiRecoveryManager::handleFileList() {
    File root = LittleFS.open("/");
    String json = "[";
    File file = root.openNextFile();
    bool first = true;
    while (file) {
        if (!first) json += ",";
        json += "{\"name\":\"" + String(file.name()) + "\",\"size\":" + String(file.size()) + "}";
        first = false;
        file = root.openNextFile();
    }
    json += "]";
    _server.send(200, "application/json", json);
}

void WiFiRecoveryManager::handleDownload() {
    if (!_server.hasArg("file")) {
        _server.send(400, "text/plain", "Missing file parameter");
        return;
    }
    String path = _server.arg("file");
    if (!LittleFS.exists(path)) {
        _server.send(404, "text/plain", "File Not Found");
        return;
    }

    File file = LittleFS.open(path, FILE_READ);
    _server.streamFile(file, "text/csv");
    file.close();
}

void WiFiRecoveryManager::handleNotFound() {
    _server.send(404, "text/plain", "404 Endpoint Not Found");
}

void WiFiRecoveryManager::printDiagnostics() const {
    Serial.printf("  Wi-Fi AP     : Status=%s | IP=%s | Endpoints: /, /status, /config, /calibration, /files, /download\n",
                  _isRunning ? "RUNNING" : "STOPPED", WiFi.softAPIP().toString().c_str());
}
