"""
Preprocessing: cleaning and preparation of raw telemetry DataFrames.

- Converts types
- Clips extreme outliers (conservative; does NOT impute missing data)
- Adds a rolling-window smoothed copy of key columns (suffix _smooth)
- Records provenance: smoothed columns are CALCULATED, raw preserved

Raw data is NEVER overwritten.  Smoothed columns are added alongside.
"""

from __future__ import annotations

import logging
from typing import Optional

import numpy as np
import pandas as pd

log = logging.getLogger(__name__)

# Columns to smooth and their window sizes (samples)
SMOOTH_COLUMNS = {
    "mag_x_ut":            5,
    "mag_y_ut":            5,
    "mag_z_ut":            5,
    "water_temp_c":        11,
    "pi_anomaly_strength": 3,
    "heading_deg":         7,
}

# Hard clip limits (values beyond these are set to NaN — likely sensor glitches)
CLIP_LIMITS = {
    "water_temp_c":       (-5.0,   50.0),
    "imu_die_temp_c":     (-40.0,  85.0),
    "heading_deg":        (0.0,   360.0),
    "roll_deg":           (-180.0, 180.0),
    "pitch_deg":          (-90.0,  90.0),
    "pi_anomaly_strength": (0.0,  1e6),
}


def preprocess(df: pd.DataFrame, station_id: str = "") -> pd.DataFrame:
    """
    Clean and enrich a normalised telemetry DataFrame.

    Returns a NEW DataFrame (raw data unchanged).
    """
    df = df.copy()

    # 1. Parse timestamps
    if "timestamp_unix" in df.columns:
        df["timestamp_unix"] = pd.to_numeric(df["timestamp_unix"], errors="coerce")

    if "timestamp_rtc" in df.columns:
        df["timestamp_rtc"] = pd.to_datetime(df["timestamp_rtc"], errors="coerce", utc=True)

    # 2. Coerce numeric columns
    numeric_cols = [
        "accel_x_g", "accel_y_g", "accel_z_g",
        "gyro_x_dps", "gyro_y_dps", "gyro_z_dps",
        "imu_die_temp_c", "water_temp_c",
        "mag_x_ut", "mag_y_ut", "mag_z_ut",
        "pi_baseline", "pi_peak", "pi_early_decay",
        "pi_late_decay", "pi_anomaly_strength",
        "roll_deg", "pitch_deg", "heading_deg",
        "drift_north_m", "drift_east_m",
        "deployment_drift_radius_m",
        "latitude", "longitude",
    ]
    for col in numeric_cols:
        if col in df.columns:
            df[col] = pd.to_numeric(df[col], errors="coerce")

    # 3. Clip extreme outliers
    for col, (lo, hi) in CLIP_LIMITS.items():
        if col in df.columns:
            before = df[col].notna().sum()
            df.loc[(df[col] < lo) | (df[col] > hi), col] = np.nan
            after = df[col].notna().sum()
            clipped = before - after
            if clipped > 0:
                log.debug("[%s] Clipped %d outliers in %s", station_id, clipped, col)

    # 4. Add rolling-smoothed columns (CALCULATED)
    for col, window in SMOOTH_COLUMNS.items():
        if col in df.columns:
            smooth_col = col + "_smooth"
            df[smooth_col] = (
                df[col]
                .rolling(window=window, center=True, min_periods=1)
                .mean()
            )

    # 5. Compute total horizontal acceleration (useful for activity detection)
    if "accel_x_g" in df.columns and "accel_y_g" in df.columns:
        df["horiz_accel_g"] = np.sqrt(df["accel_x_g"] ** 2 + df["accel_y_g"] ** 2)

    # 6. Compute magnetic field magnitude
    if all(c in df.columns for c in ("mag_x_ut", "mag_y_ut", "mag_z_ut")):
        df["mag_total_ut"] = np.sqrt(
            df["mag_x_ut"] ** 2 + df["mag_y_ut"] ** 2 + df["mag_z_ut"] ** 2
        )

    log.info("[%s] Preprocessing complete — %d rows, %d columns", station_id, len(df), len(df.columns))
    return df
