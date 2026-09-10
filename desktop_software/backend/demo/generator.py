"""
Synthetic demo-data generator.

Produces realistic (but entirely synthetic) multi-station telemetry for
SIH demonstration when no real ESP32 data is available.

All generated records are tagged DataProvenance.DEMO.
They are NEVER saved as real measurements.

Usage:
    from backend.demo.generator import generate_demo_deployment
    df, meta = generate_demo_deployment("DEMO_A", lat=15.0, lon=74.0)
"""

from __future__ import annotations

import random
import time
from datetime import datetime, timedelta, timezone
from typing import List, Tuple

import numpy as np
import pandas as pd

from backend.models.schemas import DataProvenance, DeploymentMetadata


# Default demo deployment positions (Indian Ocean / Arabian Sea region)
DEMO_STATIONS = [
    ("DEMO_A", 14.9800, 74.0100),
    ("DEMO_B", 14.9820, 74.0135),
    ("DEMO_C", 14.9805, 74.0160),
]


def _rng(seed: int) -> np.random.Generator:
    return np.random.default_rng(seed)


def generate_demo_deployment(
    station_id: str,
    lat: float,
    lon: float,
    n_samples: int = 300,
    has_anomaly: bool = True,
    seed: int = 42,
) -> Tuple[pd.DataFrame, DeploymentMetadata]:
    """
    Generate synthetic telemetry for one deployment station.

    Parameters
    ----------
    station_id  : unique station name
    lat, lon    : deployment GPS coordinates
    n_samples   : number of 0.1 Hz samples (~50 min at 10 Hz / decimated)
    has_anomaly : if True, inject a PI+magnetic anomaly cluster midway
    seed        : random seed for reproducibility

    Returns
    -------
    (DataFrame of synthetic telemetry, DeploymentMetadata)
    """
    rng = _rng(seed)
    t0 = datetime(2026, 9, 1, 8, 0, 0, tzinfo=timezone.utc)
    times = [t0 + timedelta(seconds=i * 10) for i in range(n_samples)]

    # --- Magnetic field baseline (nT / µT) --------------------------------
    # Typical total field ~40 µT; add slow drift + noise
    mag_base = 40.0 + rng.normal(0, 0.3, n_samples).cumsum() * 0.02
    mag_x = mag_base * 0.6 + rng.normal(0, 0.5, n_samples)
    mag_y = mag_base * 0.5 + rng.normal(0, 0.5, n_samples)
    mag_z = mag_base * 0.4 + rng.normal(0, 0.5, n_samples)

    # --- Water temperature (°C) ------------------------------------------
    water_temp = 4.0 + rng.normal(0, 0.1, n_samples)

    # --- IMU die temperature ---------------------------------------------
    imu_die_temp = 35.0 + rng.normal(0, 0.5, n_samples)

    # --- IMU (slow drift vehicle) ----------------------------------------
    accel_x = rng.normal(0, 0.02, n_samples)
    accel_y = rng.normal(0, 0.02, n_samples)
    accel_z = rng.normal(-1.0, 0.02, n_samples)  # gravity

    # --- PI decay baseline -----------------------------------------------
    pi_baseline       = rng.normal(0.05, 0.01, n_samples)
    pi_peak           = rng.normal(0.08, 0.01, n_samples)
    pi_early_decay    = rng.normal(0.07, 0.01, n_samples)
    pi_late_decay     = rng.normal(0.06, 0.01, n_samples)
    pi_anomaly_str    = rng.normal(0.02, 0.005, n_samples).clip(0)

    # --- Inject anomaly cluster (if requested) ---------------------------
    if has_anomaly:
        mid = n_samples // 2
        width = n_samples // 8
        anomaly_region = slice(max(0, mid - width), min(n_samples, mid + width))

        # PI spike
        pi_anomaly_str[anomaly_region] += rng.uniform(0.25, 0.45, anomaly_region.stop - anomaly_region.start)
        pi_peak[anomaly_region] += 0.15

        # Magnetic disturbance
        mag_x[anomaly_region] += rng.normal(0, 3.0, anomaly_region.stop - anomaly_region.start)
        mag_y[anomaly_region] += rng.normal(0, 3.0, anomaly_region.stop - anomaly_region.start)

        # Slight thermal anomaly
        water_temp[anomaly_region] += 0.8

    # --- Drift -----------------------------------------------------------
    heading = rng.uniform(80, 100, n_samples)   # roughly east
    drift_north = rng.normal(10, 5, n_samples).cumsum() / n_samples * 50
    drift_east  = rng.normal(15, 5, n_samples).cumsum() / n_samples * 80
    drift_radius = np.sqrt(drift_north ** 2 + drift_east ** 2)

    df = pd.DataFrame({
        "timestamp_rtc":           [t.isoformat() for t in times],
        "timestamp_unix":          [t.timestamp() for t in times],
        "imu_valid":               True,
        "pt100_valid":             True,
        "mag_valid":               True,
        "pi_valid":                True,
        "rtc_valid":               True,
        "accel_x_g":               accel_x,
        "accel_y_g":               accel_y,
        "accel_z_g":               accel_z,
        "imu_die_temp_c":          imu_die_temp,
        "water_temp_c":            water_temp,
        "mag_x_ut":                mag_x,
        "mag_y_ut":                mag_y,
        "mag_z_ut":                mag_z,
        "pi_baseline":             pi_baseline,
        "pi_peak":                 pi_peak,
        "pi_early_decay":          pi_early_decay,
        "pi_late_decay":           pi_late_decay,
        "pi_anomaly_strength":     pi_anomaly_str,
        "heading_deg":             heading,
        "drift_north_m":           drift_north,
        "drift_east_m":            drift_east,
        "deployment_drift_radius_m": drift_radius,
        "latitude":                lat,
        "longitude":               lon,
        # Demo provenance column (informational)
        "data_provenance":         DataProvenance.DEMO.value,
    })

    meta = DeploymentMetadata(
        station_id=station_id,
        deploy_latitude=lat,
        deploy_longitude=lon,
        deploy_depth_m=250.0,
        drift_scale_m=400.0,
        operator_notes="Synthetic DEMO data — not real measurements",
        demo_mode=True,
        data_provenance=DataProvenance.DEMO,
    )

    return df, meta


def generate_all_demo_stations(
    anomaly_pattern: List[bool] = None,
) -> List[Tuple[pd.DataFrame, DeploymentMetadata]]:
    """
    Generate synthetic telemetry for all default DEMO_STATIONS.

    anomaly_pattern: list of bool, one per station.
                     Defaults to [True, True, False].
    """
    if anomaly_pattern is None:
        anomaly_pattern = [True, True, False]

    results = []
    for idx, (sid, lat, lon) in enumerate(DEMO_STATIONS):
        has_anomaly = anomaly_pattern[idx] if idx < len(anomaly_pattern) else False
        df, meta = generate_demo_deployment(
            station_id=sid, lat=lat, lon=lon,
            has_anomaly=has_anomaly,
            seed=100 + idx,
        )
        results.append((df, meta))

    return results
