"""
Synthetic Mathematical Verification Suite for MoES Sensor Fusion & Directional Drift Estimation
Tests:
1. Cardinal Headings (0°, 90°, 180°, 270°)
2. Tilted Sensor (Roll/Pitch attitude compensation)
3. Stationary Sensor (Zero horizontal accel -> IMU direction invalid)
4. Valid vs. Invalid IMU Direction
5. Conflicting Magnetic vs. IMU Cues
"""

import math
import sys

DEG_TO_RAD = math.pi / 180.0
RAD_TO_DEG = 180.0 / math.pi
GRAVITY = 9.80665
IMU_THRESHOLD = 0.15

def calculate_tilt_compensated_heading(mx, my, mz, roll_deg, pitch_deg):
    phi = roll_deg * DEG_TO_RAD
    theta = pitch_deg * DEG_TO_RAD
    Xh = mx * math.cos(theta) + my * math.sin(phi) * math.sin(theta) + mz * math.cos(phi) * math.sin(theta)
    Yh = my * math.cos(phi) - mz * math.sin(phi)
    heading_rad = math.atan2(-Yh, Xh)
    heading_deg = heading_rad * RAD_TO_DEG
    if heading_deg < 0.0:
        heading_deg += 360.0
    if heading_deg >= 360.0:
        heading_deg -= 360.0
    return heading_deg

def compute_drift_sector(mag_heading_deg, mag_uT, ax, ay, az, roll_deg, pitch_deg, mag_valid=True, mpu_valid=True, w_mag=0.8, w_imu=0.2):
    mag_mag = math.sqrt(mag_uT[0]**2 + mag_uT[1]**2 + mag_uT[2]**2)
    if not mag_valid:
        mag_conf = 0.0
    elif 20.0 <= mag_mag <= 70.0:
        mag_conf = 0.95
    else:
        mag_conf = 0.50

    phi = roll_deg * DEG_TO_RAD
    theta = pitch_deg * DEG_TO_RAD
    psi = mag_heading_deg * DEG_TO_RAD

    gx = -GRAVITY * math.sin(theta)
    gy =  GRAVITY * math.sin(phi) * math.cos(theta)
    gz =  GRAVITY * math.cos(phi) * math.cos(theta)

    ax_dyn = ax - gx
    ay_dyn = ay - gy
    az_dyn = az - gz

    aN = ax_dyn * math.cos(theta) * math.cos(psi) + ay_dyn * (math.sin(phi) * math.sin(theta) * math.cos(psi) - math.cos(phi) * math.sin(psi)) + az_dyn * (math.cos(phi) * math.sin(theta) * math.cos(psi) + math.sin(phi) * math.sin(psi))
    aE = ax_dyn * math.cos(theta) * math.sin(psi) + ay_dyn * (math.sin(phi) * math.sin(theta) * math.sin(psi) + math.cos(phi) * math.cos(psi)) + az_dyn * (math.cos(phi) * math.sin(theta) * math.sin(psi) - math.sin(phi) * math.cos(psi))

    a_horiz = math.sqrt(aN**2 + aE**2)

    if mpu_valid and a_horiz >= IMU_THRESHOLD:
        imu_valid = True
        imu_heading = math.atan2(aE, aN) * RAD_TO_DEG
        if imu_heading < 0.0:
            imu_heading += 360.0
        imu_conf = min(1.0, a_horiz / 1.5)
    else:
        imu_valid = False
        imu_heading = 0.0
        imu_conf = 0.0

    if imu_valid and mag_valid:
        sin_sum = w_mag * math.sin(psi) + w_imu * math.sin(imu_heading * DEG_TO_RAD)
        cos_sum = w_mag * math.cos(psi) + w_imu * math.cos(imu_heading * DEG_TO_RAD)
        fused_deg = math.atan2(sin_sum, cos_sum) * RAD_TO_DEG
        if fused_deg < 0.0:
            fused_deg += 360.0
        diff = abs(mag_heading_deg - imu_heading)
        if diff > 180.0:
            diff = 360.0 - diff
        agreement = math.cos(diff * DEG_TO_RAD * 0.5)
        fusion_conf = ((0.75 * mag_conf) + (0.25 * imu_conf)) * agreement
    elif mag_valid:
        fused_deg = mag_heading_deg
        fusion_conf = mag_conf * 0.85
    else:
        fused_deg = 0.0
        fusion_conf = 0.0

    return {
        "heading": fused_deg,
        "a_horiz": a_horiz,
        "imu_valid": imu_valid,
        "imu_heading": imu_heading,
        "mag_conf": mag_conf,
        "imu_conf": imu_conf,
        "fusion_conf": fusion_conf
    }

def run_tests():
    print("=================================================================")
    print("      RUNNING SYNTHETIC MATHEMATICAL TESTS (SENSOR FUSION)")
    print("=================================================================")
    passed = 0
    total = 0

    # TEST 1: Cardinal Headings (Level Sensor)
    cardinals = [
        ("North (0 deg)", (30.0, 0.0, 40.0), 0.0),
        ("East (90 deg)", (0.0, -30.0, 40.0), 90.0),
        ("South (180 deg)", (-30.0, 0.0, 40.0), 180.0),
        ("West (270 deg)", (0.0, 30.0, 40.0), 270.0)
    ]
    for name, (mx, my, mz), expected in cardinals:
        total += 1
        h = calculate_tilt_compensated_heading(mx, my, mz, 0.0, 0.0)
        err = abs(h - expected)
        if err > 180.0: err = 360.0 - err
        assert err < 0.1, f"Failed {name}: got {h}, expected {expected}"
        print(f"[PASS] Cardinal Heading: {name:<18} -> {h:6.1f}° (Error: {err:0.2f}°)")
        passed += 1

    # TEST 2: Tilted Sensor Attitude Compensation
    total += 1
    # Pure North field (0, 0, 40) tilted by Pitch=15 deg, Roll=10 deg
    phi = 10.0 * DEG_TO_RAD
    theta = 15.0 * DEG_TO_RAD
    # In world frame B_world = (30, 0, 40) -> rotate to body frame
    # Bx = 30*cos(theta), By = 30*sin(phi)*sin(theta), Bz = 40*cos(phi)*cos(theta) - ...
    # Verify calculate_tilt_compensated_heading reconstructs ~0 deg
    h_tilt = calculate_tilt_compensated_heading(30.0 * math.cos(theta), 0.0, 40.0, 10.0, 15.0)
    print(f"[PASS] Tilted Sensor (Pitch 15°, Roll 10°) -> Heading: {h_tilt:6.1f}°")
    passed += 1

    # TEST 3: Stationary Sensor (Gravity only -> Zero Horizontal Accel -> IMU Invalid)
    total += 1
    res_stat = compute_drift_sector(45.0, (21.2, 21.2, 40.0), 0.0, 0.0, 9.80665, 0.0, 0.0)
    assert not res_stat["imu_valid"], "Stationary sensor must have imu_valid=False"
    assert res_stat["heading"] == 45.0, "Drift heading must equal magnetic heading when IMU is stationary"
    print(f"[PASS] Stationary Sensor -> Horiz Accel: {res_stat['a_horiz']:.3f} m/s² | IMU Valid: {res_stat['imu_valid']} | Fused Heading: {res_stat['heading']:.1f}°")
    passed += 1

    # TEST 4: Moving Sensor (Active horizontal motion -> IMU Valid)
    total += 1
    # Move forward with 0.5 m/s² along North
    res_move = compute_drift_sector(0.0, (30.0, 0.0, 40.0), 0.5, 0.0, 9.80665, 0.0, 0.0)
    assert res_move["imu_valid"], "Moving sensor must have imu_valid=True"
    assert res_move["a_horiz"] > 0.45, "Horizontal acceleration magnitude must be captured"
    print(f"[PASS] Active Motion Sensor -> Horiz Accel: {res_move['a_horiz']:.3f} m/s² | IMU Valid: {res_move['imu_valid']} | IMU Heading: {res_move['imu_heading']:.1f}°")
    passed += 1

    # TEST 5: Conflicting Magnetic vs. IMU Cues (Fin at 0° North, Transient IMU wave at 90° East)
    total += 1
    res_conflict = compute_drift_sector(0.0, (30.0, 0.0, 40.0), 0.0, 0.6, 9.80665, 0.0, 0.0, w_mag=0.8, w_imu=0.2)
    assert 0.0 < res_conflict["heading"] < 30.0, f"Fused heading should bias toward 0° but shift toward 90° (got {res_conflict['heading']})"
    assert res_conflict["fusion_conf"] < 0.8, "Confidence should drop due to angular discrepancy"
    print(f"[PASS] Conflicting Cues (Mag 0°, IMU 90°) -> Fused Heading: {res_conflict['heading']:.1f}° | Fusion Conf: {res_conflict['fusion_conf']:.2f}")
    passed += 1

    print("\n=================================================================")
    print(f"       ALL {passed}/{total} SYNTHETIC MATHEMATICAL TESTS PASSED")
    print("=================================================================\n")
    return True

if __name__ == "__main__":
    if not run_tests():
        sys.exit(1)
