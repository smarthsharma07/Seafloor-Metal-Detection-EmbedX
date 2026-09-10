#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <SPI.h>

// ============================================================================
// RUNTIME MODES
// Set DEMO_MODE to 1 to run full simulation without physical sensors attached
// ============================================================================
#ifndef DEMO_MODE
#define DEMO_MODE                   0   // 0 = Physical Hardware, 1 = Synthetic Simulation
#endif

// ============================================================================
// HARDWARE PIN DEFINITIONS (Reconciled against supplied schematic)
// ============================================================================

// --- CONFIRMED PINS (Explicitly specified in project hardware schematic) ---
#define SPI_MISO_PIN                2   // EXT1 Pin 3 (CONFIRMED)
#define RM3100_CS_PIN               4   // EXT1 Pin 4 (CONFIRMED, Active LOW)
#define MAX31865_CS_PIN             5   // EXT1 Pin 5 (CONFIRMED, Active LOW)
#define I2C_SDA_PIN                 13  // EXT2 Pin 3 (CONFIRMED)
#define SPI_SCK_PIN                 14  // EXT2 Pin 7 (CONFIRMED)
#define SPI_MOSI_PIN                15  // EXT2 Pin 8 (CONFIRMED)
#define PI_ADC_INPUT_PIN            32  // EXT2 Pin 6 (CONFIRMED, ADC1_CH4)
#define PI_MOSFET_GATE_PIN          33  // EXT2 Pin 5 (CONFIRMED, Digital Output)

// --- HARDWARE_TBD PINS (Missing or unconfirmed in supplied schematic) ---
#define I2C_SCL_PIN                 16  // HARDWARE_TBD (Prompt specifies GPIO?, default 16 for Olimex EXT2)
#define RM3100_DRDY_PIN             -1  // HARDWARE_TBD (Unassigned on ESP32, polling mode used)

// ============================================================================
// BUS CONFIGURATION & SPEEDS
// ============================================================================
#define I2C_BUS_SPEED               400000U // 400 kHz Fast-Mode I2C

// RM3100: SPI Mode 0 (CPOL=0, CPHA=0), 1 MHz max speed
#define RM3100_SPI_FREQ             1000000U
#define RM3100_SPI_MODE             SPI_MODE0

// MAX31865: SPI Mode 1 (CPOL=0, CPHA=1), 1 MHz max speed
#define MAX31865_SPI_FREQ           1000000U
#define MAX31865_SPI_MODE           SPI_MODE1

// ============================================================================
// I2C ADDRESS DEFINITIONS
// ============================================================================
#define DS3231_I2C_ADDR             0x68 // Fixed DS3231 RTC Address
#define MPU6050_I2C_ADDR_DEFAULT    0x69 // Configurable (Default 0x69, AD0 tied to 3.3V)

// ============================================================================
// SENSOR BODY FRAME & GLOBAL FRAME CONVENTIONS
// ============================================================================
// Configurable coordinate frame mapping to support physical sensor orientation
// Default Body Frame: X = Forward (along cylinder), Y = Port (Left), Z = Down
// Global Frame: X = North (0 deg), Y = East (90 deg), Z = Down
#define BODY_AXIS_X_SIGN            ( 1.0f)
#define BODY_AXIS_Y_SIGN            ( 1.0f)
#define BODY_AXIS_Z_SIGN            ( 1.0f)

// ============================================================================
// SENSOR & FUSION PARAMETERS
// ============================================================================
// MAX31865 RTD Parameters (UNVERIFIED BENCH_TBD)
#define MAX31865_RREF_OHMS          430.0f // Subject to bench measurement of 7Semi board
#define MAX31865_RNOMINAL_OHMS      100.0f // PT100 0°C nominal resistance

// IMU Attitude Filter
#define ATTITUDE_ALPHA              0.98f  // Complementary filter weight

// IMU Motion Threshold for Direction Validity
#define IMU_MOTION_THRESHOLD_M_S2   0.15f  // Min horizontal acceleration to consider IMU direction valid

// Fusion Weights & Sector Modeling Parameters
#define FUSION_WEIGHT_MAG           0.80f
#define FUSION_WEIGHT_IMU           0.20f
#define DEFAULT_DRIFT_RADIUS_M      400.0f // Renamed from uncertainty radius to deployment drift radius
#define DEFAULT_UNCERTAINTY_DEG     30.0f  // Half-angle uncertainty sector (total cone = 60 deg)

// ============================================================================
// PROVISIONAL PULSE INDUCTION (PI) PARAMETERS (BENCH_TBD)
// ============================================================================
#define PI_PULSE_WIDTH_US           150   // Pulse duration MOSFET Gate ON
#define PI_SETTLING_DELAY_US        30    // Flyback clamp settling delay
#define PI_PULSE_PERIOD_US         1000   // Repetition period between pulses
#define PI_SAMPLE_COUNT              16   // Number of microsecond samples per decay curve
#define PI_AVERAGING_COUNT           20   // Number of pulses averaged per burst
#define PI_BASELINE_ALPHA          0.05f  // Baseline EMA tracking rate
#define PI_DEFAULT_THRESHOLD       50.0f  // Default anomaly detection threshold (ADC counts)
#define PI_THRESHOLD_SIGMA_MULT     3.5f  // Multiplier for noise-derived threshold during calibration

// ============================================================================
// CALIBRATION DATA STRUCTURES
// ============================================================================
struct MagCalibrationData {
    float offset_x;
    float offset_y;
    float offset_z;
    float scale_x;
    float scale_y;
    float scale_z;
    bool is_calibrated;
};

struct ImuCalibrationData {
    float accel_offset_x;
    float accel_offset_y;
    float accel_offset_z;
    float gyro_offset_x;
    float gyro_offset_y;
    float gyro_offset_z;
    bool is_calibrated;
};

struct PiCalibrationData {
    float baseline_counts;
    float noise_sigma;
    float derived_threshold;
    bool is_calibrated;
};

// ============================================================================
// UNIFIED SENSOR SAMPLE RECORD (Raw, Processed, and Validity Separated)
// ============================================================================
struct SensorSample {
    // --- 1. TIMESTAMPS & RECORD INFO ---
    uint32_t timestamp_epoch;            // DS3231 Unix timestamp (seconds)
    uint32_t timestamp_us;               // High-resolution boot microseconds
    char iso_timestamp[32];              // Formatted ISO-8601 UTC string

    // --- 2. SENSOR VALIDITY FLAGS ---
    bool mpu_valid;                      // True if MPU6050 returned valid data
    bool mag_valid;                      // True if RM3100 returned valid data
    bool rtd_valid;                      // True if MAX31865 reading valid & no fault
    bool rtc_valid;                      // True if DS3231 communication & time valid
    bool pi_valid;                       // True if PI acquisition succeeded
    bool imu_direction_valid;            // True if horizontal acceleration exceeds threshold

    // --- 3. RAW SENSOR MEASUREMENTS ---
    int16_t raw_accel_x, raw_accel_y, raw_accel_z; // Raw MPU6050 LSBs
    int16_t raw_gyro_x, raw_gyro_y, raw_gyro_z;    // Raw Gyro LSBs
    int16_t raw_mpu_temp;                          // Raw MPU6050 die temp LSBs
    int32_t raw_mag_x, raw_mag_y, raw_mag_z;       // Raw RM3100 counts (24-bit)
    uint16_t raw_rtd_count;                        // Raw MAX31865 15-bit RTD count
    uint8_t rtd_fault_byte;                        // MAX31865 fault register
    uint16_t pi_raw_decay[PI_SAMPLE_COUNT];        // Raw PI microsecond decay waveform

    // --- 4. PROCESSED SENSOR DATA ---
    float water_temperature_C;           // Seawater temperature via MAX31865 PT100 probe
    float rtd_resistance_ohms;           // Measured PT100 resistance
    float imu_die_temperature_C;         // Internal MPU6050 die temperature
    float accel_x_m_s2;                  // Calibrated acceleration X
    float accel_y_m_s2;                  // Calibrated acceleration Y
    float accel_z_m_s2;                  // Calibrated acceleration Z
    float gyro_x_rad_s;                  // Calibrated gyroscope X
    float gyro_y_rad_s;                  // Calibrated gyroscope Y
    float gyro_z_rad_s;                  // Calibrated gyroscope Z
    float roll_deg;                      // Estimated roll angle
    float pitch_deg;                     // Estimated pitch angle
    float mag_x_uT;                      // Calibrated magnetic field X
    float mag_y_uT;                      // Calibrated magnetic field Y
    float mag_z_uT;                      // Calibrated magnetic field Z
    float magnetic_heading_deg;          // Tilt-compensated heading (0-360 deg)

    // --- 5. PI DECAY METRICS ---
    float pi_baseline;                   // Background zero-metal decay baseline
    float pi_peak;                       // Peak amplitude of decay waveform
    float pi_early_decay;                // Mean of early decay samples (fast transient)
    float pi_late_decay;                 // Mean of late decay samples (eddy current decay)
    float pi_anomaly_strength;           // Anomaly magnitude above baseline
    bool pi_detected;                    // True if anomaly strength exceeds threshold

    // --- 6. SENSOR FUSION & DIRECTIONAL DRIFT MODEL ---
    float accel_horiz_mag;               // Gravity-compensated horizontal acceleration
    float imu_drift_heading_deg;         // Direction derived from short-term IMU motion
    float estimated_drift_bearing_deg;   // Fused dominant drift bearing
    float uncertainty_angle_deg;         // Sector half-angle (e.g. +/-30 deg)
    float deployment_drift_radius_m;     // Operational drift radius (400 m)
    float magnetic_confidence;           // Magnetic reading confidence (0.0 to 1.0)
    float imu_confidence;                // IMU motion signal confidence (0.0 to 1.0)
    float fusion_confidence;             // Combined fusion confidence score (0.0 to 1.0)
};

#endif // CONFIG_H
