#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>

#include "config.h"
#include "sensors/mpu6050.h"
#include "sensors/max31865_pt100.h"
#include "sensors/rm3100.h"
#include "pi/pi_detector.h"
#include "fusion/attitude.h"
#include "fusion/heading.h"
#include "fusion/drift_fusion.h"
#include "logging/logger.h"
#include "communication/wifi_manager.h"
#include "diagnostics/diagnostics.h"

// Global Hardware Instances
MPU6050Sensor       g_mpu(MPU6050_I2C_ADDR_DEFAULT);
DS3231RTC           g_rtc;
MAX31865Sensor      g_rtd(MAX31865_CS_PIN);
RM3100Sensor        g_mag(RM3100_CS_PIN);
PIDetector          g_pi(PI_MOSFET_GATE_PIN, PI_ADC_INPUT_PIN);

// Global Processing & Communication Instances
AttitudeFilter      g_attitude;
HeadingCalculator   g_heading;
DriftFusionEngine   g_driftFusion;
TelemetryLogger     g_logger;
WiFiRecoveryManager g_wifi;

// Global Shared Data Record
SensorSample        g_latestSample;

// Runtime Flags
static bool g_demoModeActive = (DEMO_MODE == 1);
static uint32_t lastLoopTimeMs = 0;
static uint32_t lastSampleTimeMs = 0;
static uint32_t simStepCount = 0;

// Forward Declarations
void processSerialCLI();
void executeMagCal();
void executeImuCal();
void executePiRead();
void executePiCal();
void runSimulationStep(float dt_s);

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n=======================================================");
    Serial.println("  MoES Seafloor Metal Detection Sensor Package Firmware");
    Serial.println("  Problem Statement ID: 26064 (MoES / NCPOR)");
    Serial.println("  PROTOTYPE SCOPE: Bench Bring-Up / Field Drift Estimator");
    Serial.println("=======================================================");

    // 1. Initialize I2C Bus
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, I2C_BUS_SPEED);
    Serial.printf("[SYSTEM] I2C Bus Initialized (SDA=GPIO%u, SCL=GPIO%u [HARDWARE_TBD], Speed=%u Hz)\n",
                  I2C_SDA_PIN, I2C_SCL_PIN, I2C_BUS_SPEED);

    // 2. Initialize Shared SPI Bus
    SPI.begin(SPI_SCK_PIN, SPI_MISO_PIN, SPI_MOSI_PIN);
    Serial.printf("[SYSTEM] Shared SPI Bus Initialized (SCK=GPIO%u, MISO=GPIO%u, MOSI=GPIO%u)\n",
                  SPI_SCK_PIN, SPI_MISO_PIN, SPI_MOSI_PIN);

    // 3. Bring Up Peripherals with Strict Order & Startup Identity Checks
    // DS3231 RTC checked first at fixed address 0x68
    g_rtc.begin(Wire, DS3231_I2C_ADDR);

    // MPU6050 strictly checked at configured address 0x69 (no fallback to 0x68)
    g_mpu.begin(Wire, MPU6050_I2C_ADDR_DEFAULT);

    // MAX31865 RTD converter on SPI CS GPIO5 (SPI Mode 1)
    g_rtd.begin(SPI, MAX31865_RREF_OHMS, MAX31865_RNOMINAL_OHMS);

    // RM3100 Magnetometer on SPI CS GPIO4 (SPI Mode 0)
    g_mag.begin(SPI);

    // Parameterized Pulse Induction Engine (Gate GPIO33, ADC GPIO32)
    g_pi.begin();

    // 4. Initialize Data Storage & Recovery Server
    g_logger.begin();
    g_wifi.begin("MoES_Sensor_Recovery", "Seafloor2026");

    // 5. Print Boot Hardware Status Table
    SystemDiagnostics::printSelfTestTable(g_mpu, g_rtc, g_rtd, g_mag, g_pi, g_logger, g_wifi);

    if (g_demoModeActive) {
        Serial.println("[SYSTEM] DEMO_MODE is ENABLED. Generating synthetic telemetry.");
    }

    Serial.println("System Ready. Type 'help' for interactive CLI commands.");
    lastLoopTimeMs = millis();
}

void loop() {
    uint32_t nowMs = millis();
    float dt_s = (nowMs - lastLoopTimeMs) / 1000.0f;
    if (dt_s <= 0.0f) dt_s = 0.01f;
    lastLoopTimeMs = nowMs;

    // Handle background Wi-Fi HTTP Server requests
    g_wifi.handleClient();

    // Handle Serial Console CLI input
    if (Serial.available()) {
        processSerialCLI();
    }

    // Time-Multiplexed Sensor Acquisition Loop (100 ms = 10 Hz rate)
    if (nowMs - lastSampleTimeMs >= 100) {
        lastSampleTimeMs = nowMs;

        if (g_demoModeActive) {
            runSimulationStep(dt_s);
        } else {
            // --- STEP A: RTC TIME ---
            g_latestSample.rtc_valid = g_rtc.readTime(g_latestSample.timestamp_epoch,
                                                      g_latestSample.iso_timestamp,
                                                      sizeof(g_latestSample.iso_timestamp));
            g_latestSample.timestamp_us = micros();

            // --- STEP B: SEAWATER RTD TEMPERATURE ---
            g_latestSample.rtd_valid = g_rtd.readScaled(g_latestSample.water_temperature_C,
                                                        g_latestSample.rtd_resistance_ohms,
                                                        g_latestSample.rtd_fault_byte);

            // --- STEP C: MPU6050 MOTION & DIE TEMPERATURE ---
            g_latestSample.mpu_valid = g_mpu.readScaled(g_latestSample.accel_x_m_s2,
                                                        g_latestSample.accel_y_m_s2,
                                                        g_latestSample.accel_z_m_s2,
                                                        g_latestSample.gyro_x_rad_s,
                                                        g_latestSample.gyro_y_rad_s,
                                                        g_latestSample.gyro_z_rad_s,
                                                        g_latestSample.imu_die_temperature_C);
            if (g_latestSample.mpu_valid) {
                g_attitude.update(g_latestSample.accel_x_m_s2,
                                  g_latestSample.accel_y_m_s2,
                                  g_latestSample.accel_z_m_s2,
                                  g_latestSample.gyro_x_rad_s,
                                  g_latestSample.gyro_y_rad_s,
                                  g_latestSample.gyro_z_rad_s,
                                  dt_s,
                                  g_latestSample.roll_deg,
                                  g_latestSample.pitch_deg);
            }

            // --- STEP D: RM3100 MAGNETOMETER (Time Window: PI OFF) ---
            g_latestSample.mag_valid = g_mag.readScaled(g_latestSample.mag_x_uT,
                                                        g_latestSample.mag_y_uT,
                                                        g_latestSample.mag_z_uT);
            if (g_latestSample.mag_valid) {
                g_latestSample.magnetic_heading_deg = g_heading.calculateHeading(
                    g_latestSample.mag_x_uT, g_latestSample.mag_y_uT, g_latestSample.mag_z_uT,
                    g_latestSample.roll_deg, g_latestSample.pitch_deg);
            }

            // --- STEP E: PULSE INDUCTION (Time Window: Mag read complete) ---
            g_latestSample.pi_valid = g_pi.processPulseSequence(
                g_latestSample.pi_raw_decay,
                g_latestSample.pi_baseline,
                g_latestSample.pi_peak,
                g_latestSample.pi_early_decay,
                g_latestSample.pi_late_decay,
                g_latestSample.pi_anomaly_strength,
                g_latestSample.pi_detected);

            // --- STEP F: SENSOR FUSION & CONDITIONAL DRIFT SECTOR ---
            float magMag = sqrtf(g_latestSample.mag_x_uT * g_latestSample.mag_x_uT +
                                 g_latestSample.mag_y_uT * g_latestSample.mag_y_uT +
                                 g_latestSample.mag_z_uT * g_latestSample.mag_z_uT);

            g_driftFusion.computeDriftSector(
                g_latestSample.magnetic_heading_deg, magMag,
                g_latestSample.accel_x_m_s2, g_latestSample.accel_y_m_s2, g_latestSample.accel_z_m_s2,
                g_latestSample.roll_deg, g_latestSample.pitch_deg,
                g_latestSample.mag_valid, g_latestSample.mpu_valid,
                g_latestSample.accel_horiz_mag,
                g_latestSample.imu_drift_heading_deg,
                g_latestSample.imu_direction_valid,
                g_latestSample.estimated_drift_bearing_deg,
                g_latestSample.uncertainty_angle_deg,
                g_latestSample.deployment_drift_radius_m,
                g_latestSample.magnetic_confidence,
                g_latestSample.imu_confidence,
                g_latestSample.fusion_confidence);
        }

        // --- STEP G: LOG TELEMETRY SAMPLE ---
        g_logger.logSample(g_latestSample);
    }
}

void runSimulationStep(float dt_s) {
    simStepCount++;
    float t = simStepCount * 0.1f; // Simulation time in seconds

    // 1. Timestamps
    g_latestSample.timestamp_epoch = 1788780000 + (uint32_t)t;
    g_latestSample.timestamp_us = micros();
    snprintf(g_latestSample.iso_timestamp, sizeof(g_latestSample.iso_timestamp),
             "2026-09-07T14:30:%02u.%03uZ", (uint32_t)t % 60, (uint32_t)(t * 1000) % 1000);

    // 2. Validity Flags
    g_latestSample.mpu_valid = true;
    g_latestSample.mag_valid = true;
    g_latestSample.rtd_valid = true;
    g_latestSample.rtc_valid = true;
    g_latestSample.pi_valid  = true;

    // 3. Simulated Seawater & Die Temperatures
    g_latestSample.water_temperature_C = 4.2f + 0.1f * sinf(t * 0.05f); // Deep sea ~4°C
    g_latestSample.rtd_resistance_ohms = 101.64f;
    g_latestSample.rtd_fault_byte = 0;
    g_latestSample.imu_die_temperature_C = 28.5f + 0.2f * sinf(t * 0.02f);

    // 4. Simulated Vessel Motion & Attitude (Gentle oscillating roll & pitch)
    g_latestSample.roll_deg = 3.5f * sinf(t * 0.8f);
    g_latestSample.pitch_deg = 2.0f * cosf(t * 0.6f);

    // Simulated horizontal transient acceleration (bursts every 10 seconds)
    float horizAccel = 0.05f;
    if (fmodf(t, 10.0f) < 2.5f) {
        horizAccel = 0.35f; // Active motion burst
    }
    g_latestSample.accel_x_m_s2 = horizAccel * cosf(t * 0.1f);
    g_latestSample.accel_y_m_s2 = horizAccel * sinf(t * 0.1f);
    g_latestSample.accel_z_m_s2 = 9.81f;
    g_latestSample.gyro_x_rad_s = 0.02f * cosf(t * 0.8f);
    g_latestSample.gyro_y_rad_s = -0.01f * sinf(t * 0.6f);
    g_latestSample.gyro_z_rad_s = 0.005f;

    // 5. Simulated Magnetic Field (Dominant North-East Drift at ~45 degrees + small fin oscillation)
    float baseHeading = 45.0f + 5.0f * sinf(t * 0.2f);
    g_latestSample.magnetic_heading_deg = baseHeading;
    g_latestSample.mag_x_uT = 30.0f * cosf(baseHeading * (3.14159f / 180.0f));
    g_latestSample.mag_y_uT = 30.0f * sinf(baseHeading * (3.14159f / 180.0f));
    g_latestSample.mag_z_uT = 38.0f;

    // 6. Simulated PI Decay & Periodic Metal Anomaly Injection
    g_latestSample.pi_baseline = 120.0f;
    bool injectMetal = (fmodf(t, 15.0f) < 3.0f); // Metal anomaly every 15s
    float anomalyMag = injectMetal ? 180.0f : 5.0f;

    g_latestSample.pi_peak = g_latestSample.pi_baseline + anomalyMag;
    g_latestSample.pi_early_decay = g_latestSample.pi_baseline + (anomalyMag * 0.7f);
    g_latestSample.pi_late_decay  = g_latestSample.pi_baseline + (anomalyMag * 0.2f);
    g_latestSample.pi_anomaly_strength = anomalyMag;
    g_latestSample.pi_detected = (anomalyMag >= PI_DEFAULT_THRESHOLD);

    for (int i = 0; i < PI_SAMPLE_COUNT; i++) {
        g_latestSample.pi_raw_decay[i] = (uint16_t)(g_latestSample.pi_peak * expf(-i * 0.2f));
    }

    // 7. Sensor Fusion
    float magMag = sqrtf(g_latestSample.mag_x_uT * g_latestSample.mag_x_uT +
                         g_latestSample.mag_y_uT * g_latestSample.mag_y_uT +
                         g_latestSample.mag_z_uT * g_latestSample.mag_z_uT);

    g_driftFusion.computeDriftSector(
        g_latestSample.magnetic_heading_deg, magMag,
        g_latestSample.accel_x_m_s2, g_latestSample.accel_y_m_s2, g_latestSample.accel_z_m_s2,
        g_latestSample.roll_deg, g_latestSample.pitch_deg,
        g_latestSample.mag_valid, g_latestSample.mpu_valid,
        g_latestSample.accel_horiz_mag,
        g_latestSample.imu_drift_heading_deg,
        g_latestSample.imu_direction_valid,
        g_latestSample.estimated_drift_bearing_deg,
        g_latestSample.uncertainty_angle_deg,
        g_latestSample.deployment_drift_radius_m,
        g_latestSample.magnetic_confidence,
        g_latestSample.imu_confidence,
        g_latestSample.fusion_confidence);
}

void executePiRead() {
    Serial.println("\n[PI_READ] Triggering live Pulse Induction diagnostic read...");
    uint16_t decayCurve[PI_SAMPLE_COUNT] = {0};
    uint16_t avg = g_pi.sampleSinglePulse(decayCurve, PI_SAMPLE_COUNT);

    Serial.printf("  Decay Waveform Samples (16 microsecond points): [");
    for (int i = 0; i < PI_SAMPLE_COUNT; i++) {
        Serial.printf("%u%s", decayCurve[i], (i < PI_SAMPLE_COUNT - 1) ? ", " : "");
    }
    Serial.println("]");

    float baseline, peak, early, late, strength;
    bool detected;
    g_pi.processPulseSequence(decayCurve, baseline, peak, early, late, strength, detected);

    Serial.printf("  Burst Metrics: Peak=%.1f | Early=%.1f | Late=%.1f | Baseline=%.1f | Anomaly Strength=%.1f\n",
                  peak, early, late, baseline, strength);
    Serial.printf("  Metal Anomaly Status: %s\n\n", detected ? ">>> METAL DETECTED <<<" : "CLEAR (NO TARGET)");
}

void executePiCal() {
    Serial.println("\n=======================================================");
    Serial.println("     PULSE INDUCTION NO-TARGET BASELINE CALIBRATION");
    Serial.println("=======================================================");
    Serial.println("Ensure coil is away from all metallic objects.");
    Serial.println("Collecting 20 baseline pulse sequences to estimate noise floor...\n");

    PiCalibrationData cal;
    if (g_pi.calibrateNoTarget(20, cal)) {
        Serial.printf("  Measured Baseline : %.1f ADC counts\n", cal.baseline_counts);
        Serial.printf("  Noise Sigma (RMS) : %.2f ADC counts\n", cal.noise_sigma);
        Serial.printf("  Derived Threshold : %.1f ADC counts (%.1f x sigma)\n", cal.derived_threshold, PI_THRESHOLD_SIGMA_MULT);
        Serial.println("[PI_CAL] Calibration stored and active.\n");
    } else {
        Serial.println("[PI_CAL] ERROR: Calibration failed.\n");
    }
}

void executeMagCal() {
    Serial.println("\n=======================================================");
    Serial.println("         RM3100 3D MAGNETOMETER CALIBRATION");
    Serial.println("=======================================================");
    Serial.println("Rotate sensor package slowly through all 3D orientations.");
    Serial.println("Collecting samples for 15 seconds...\n");

    float minX = 9999.0f, maxX = -9999.0f;
    float minY = 9999.0f, maxY = -9999.0f;
    float minZ = 9999.0f, maxZ = -9999.0f;

    uint32_t startMs = millis();
    uint32_t sampleCount = 0;

    while (millis() - startMs < 15000) {
        float mx, my, mz;
        if (g_mag.readScaled(mx, my, mz)) {
            if (mx < minX) minX = mx;
            if (mx > maxX) maxX = mx;
            if (my < minY) minY = my;
            if (my > maxY) maxY = my;
            if (mz < minZ) minZ = mz;
            if (mz > maxZ) maxZ = mz;
            sampleCount++;
        }
        if (sampleCount % 10 == 0) {
            Serial.printf("  Samples: %u | X:[%.1f, %.1f] Y:[%.1f, %.1f] Z:[%.1f, %.1f]\r",
                          sampleCount, minX, maxX, minY, maxY, minZ, maxZ);
        }
        delay(50);
    }

    Serial.println("\n\nCalculating Hard-Iron & Soft-Iron Offsets...");
    MagCalibrationData cal;
    cal.offset_x = (maxX + minX) / 2.0f;
    cal.offset_y = (maxY + minY) / 2.0f;
    cal.offset_z = (maxZ + minZ) / 2.0f;

    float deltaX = (maxX - minX) / 2.0f;
    float deltaY = (maxY - minY) / 2.0f;
    float deltaZ = (maxZ - minZ) / 2.0f;
    float avgDelta = (deltaX + deltaY + deltaZ) / 3.0f;

    cal.scale_x = (deltaX > 0.1f) ? (avgDelta / deltaX) : 1.0f;
    cal.scale_y = (deltaY > 0.1f) ? (avgDelta / deltaY) : 1.0f;
    cal.scale_z = (deltaZ > 0.1f) ? (avgDelta / deltaZ) : 1.0f;
    cal.is_calibrated = true;

    g_heading.setCalibration(cal);

    Serial.printf("  Offset (uT) : X=%.2f, Y=%.2f, Z=%.2f\n", cal.offset_x, cal.offset_y, cal.offset_z);
    Serial.printf("  Scale Multi : X=%.3f, Y=%.3f, Z=%.3f\n", cal.scale_x, cal.scale_y, cal.scale_z);
    Serial.println("[MAG_CAL] Calibration applied to active Heading Calculator.\n");
}

void executeImuCal() {
    Serial.println("\n=======================================================");
    Serial.println("           MPU6050 ZERO-MOTION IMU CALIBRATION");
    Serial.println("=======================================================");
    Serial.println("Keep sensor COMPLETELY STILL on a level surface.");
    Serial.println("Collecting 200 samples over 4 seconds...\n");

    float sumAx = 0, sumAy = 0, sumAz = 0;
    float sumGx = 0, sumGy = 0, sumGz = 0;
    const uint16_t numSamples = 200;

    for (uint16_t i = 0; i < numSamples; i++) {
        float ax, ay, az, gx, gy, gz, t;
        g_mpu.readScaled(ax, ay, az, gx, gy, gz, t);
        sumAx += ax; sumAy += ay; sumAz += az;
        sumGx += gx; sumGy += gy; sumGz += gz;
        delay(20);
    }

    ImuCalibrationData cal;
    cal.accel_offset_x = sumAx / numSamples;
    cal.accel_offset_y = sumAy / numSamples;
    cal.accel_offset_z = (sumAz / numSamples) - 9.80665f; // Zero out 1g gravity on Z
    cal.gyro_offset_x  = sumGx / numSamples;
    cal.gyro_offset_y  = sumGy / numSamples;
    cal.gyro_offset_z  = sumGz / numSamples;
    cal.is_calibrated  = true;

    g_mpu.setCalibration(cal);

    Serial.printf("  Accel Offsets (m/s^2): X=%.3f, Y=%.3f, Z=%.3f\n", cal.accel_offset_x, cal.accel_offset_y, cal.accel_offset_z);
    Serial.printf("  Gyro Offsets (rad/s) : X=%.4f, Y=%.4f, Z=%.4f\n", cal.gyro_offset_x, cal.gyro_offset_y, cal.gyro_offset_z);
    Serial.println("[IMU_CAL] Calibration applied to active MPU6050 instance.\n");
}

void processSerialCLI() {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd.length() == 0) return;

    if (cmd == "help") {
        Serial.println("\n--- MoES CLI Command Help ---");
        Serial.println("  status       : Print system diagnostic hardware table");
        Serial.println("  i2cscan      : Perform I2C bus scanner probe");
        Serial.println("  readimu      : Read MPU6050 accelerometer, gyro & die temp");
        Serial.println("  readmag      : Read RM3100 magnetic vector & heading");
        Serial.println("  readtemp     : Read MAX31865 seawater RTD temperature");
        Serial.println("  readrtc      : Read DS3231 timestamp");
        Serial.println("  piread       : Fire live PI pulse burst & display decay curve");
        Serial.println("  pical        : Run no-target noise-sigma PI calibration");
        Serial.println("  magcal       : Run 15-second interactive 3D magnetometer calibration");
        Serial.println("  imucal       : Run zero-motion IMU bias calibration");
        Serial.println("  demo         : Toggle DEMO_MODE synthetic simulation");
        Serial.println("  wifi         : Display Wi-Fi Access Point IP & endpoint status");
        Serial.println("  reboot       : Soft restart ESP32 MCU\n");
    } else if (cmd == "status") {
        SystemDiagnostics::printSelfTestTable(g_mpu, g_rtc, g_rtd, g_mag, g_pi, g_logger, g_wifi);
    } else if (cmd == "i2cscan") {
        SystemDiagnostics::scanI2CBus(Wire);
    } else if (cmd == "readimu") {
        Serial.printf("IMU: Valid=%d | Accel=(%.2f, %.2f, %.2f) m/s^2 | Gyro=(%.3f, %.3f, %.3f) rad/s | Die Temp=%.1f°C | Roll=%.1f° Pitch=%.1f°\n",
                      g_latestSample.mpu_valid,
                      g_latestSample.accel_x_m_s2, g_latestSample.accel_y_m_s2, g_latestSample.accel_z_m_s2,
                      g_latestSample.gyro_x_rad_s, g_latestSample.gyro_y_rad_s, g_latestSample.gyro_z_rad_s,
                      g_latestSample.imu_die_temperature_C,
                      g_latestSample.roll_deg, g_latestSample.pitch_deg);
    } else if (cmd == "readmag") {
        Serial.printf("MAG: Valid=%d | Vector=(%.2f, %.2f, %.2f) uT | Heading=%.1f° | Mag Conf=%.2f\n",
                      g_latestSample.mag_valid,
                      g_latestSample.mag_x_uT, g_latestSample.mag_y_uT, g_latestSample.mag_z_uT,
                      g_latestSample.magnetic_heading_deg, g_latestSample.magnetic_confidence);
    } else if (cmd == "readtemp") {
        Serial.printf("RTD: Valid=%d | Water Temp=%.2f °C | Resistance=%.2f ohms | Fault=0x%02X\n",
                      g_latestSample.rtd_valid,
                      g_latestSample.water_temperature_C, g_latestSample.rtd_resistance_ohms, g_latestSample.rtd_fault_byte);
    } else if (cmd == "readrtc") {
        Serial.printf("RTC: Valid=%d | Epoch=%u | ISO String=%s\n",
                      g_latestSample.rtc_valid,
                      g_latestSample.timestamp_epoch, g_latestSample.iso_timestamp);
    } else if (cmd == "piread") {
        executePiRead();
    } else if (cmd == "pical") {
        executePiCal();
    } else if (cmd == "magcal") {
        executeMagCal();
    } else if (cmd == "imucal") {
        executeImuCal();
    } else if (cmd == "demo") {
        g_demoModeActive = !g_demoModeActive;
        Serial.printf("DEMO_MODE is now %s\n", g_demoModeActive ? "ENABLED (Simulation Active)" : "DISABLED (Hardware Active)");
    } else if (cmd == "wifi") {
        g_wifi.printDiagnostics();
    } else if (cmd == "reboot") {
        Serial.println("Rebooting ESP32...");
        ESP.restart();
    } else {
        Serial.printf("Unknown command '%s'. Type 'help' for command list.\n", cmd.c_str());
    }
}
