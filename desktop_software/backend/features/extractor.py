"""
Feature extraction: compute per-deployment statistical summaries.

All outputs are tagged DataProvenance.CALCULATED.
"""

from __future__ import annotations

import logging
from typing import List

import numpy as np
import pandas as pd

from backend.models.schemas import DataProvenance, SensorFeatures

log = logging.getLogger(__name__)

# Channels to summarise
FEATURE_CHANNELS = [
    "water_temp_c",
    "imu_die_temp_c",
    "mag_x_ut",
    "mag_y_ut",
    "mag_z_ut",
    "mag_total_ut",
    "pi_anomaly_strength",
    "pi_baseline",
    "pi_peak",
    "pi_early_decay",
    "pi_late_decay",
    "heading_deg",
    "roll_deg",
    "pitch_deg",
    "horiz_accel_g",
    "deployment_drift_radius_m",
]


def extract_features(df: pd.DataFrame, station_id: str) -> List[SensorFeatures]:
    """
    Compute statistical features for all available sensor channels.

    Parameters
    ----------
    df         : preprocessed telemetry DataFrame
    station_id : station identifier

    Returns
    -------
    List of SensorFeatures (one per channel present in df)
    """
    features: List[SensorFeatures] = []

    for channel in FEATURE_CHANNELS:
        if channel not in df.columns:
            continue
        series = pd.to_numeric(df[channel], errors="coerce").dropna()
        n_valid = len(series)

        if n_valid == 0:
            feat = SensorFeatures(
                station_id=station_id,
                channel=channel,
                n_samples=len(df),
                n_valid=0,
                data_provenance=DataProvenance.CALCULATED,
            )
        else:
            feat = SensorFeatures(
                station_id=station_id,
                channel=channel,
                n_samples=len(df),
                n_valid=n_valid,
                mean=float(series.mean()),
                std=float(series.std()),
                min_val=float(series.min()),
                max_val=float(series.max()),
                p5=float(np.percentile(series, 5)),
                p95=float(np.percentile(series, 95)),
                data_provenance=DataProvenance.CALCULATED,
            )

        features.append(feat)
        log.debug("[%s] %s: n=%d mean=%.4g std=%.4g",
                  station_id, channel, n_valid,
                  feat.mean or 0, feat.std or 0)

    return features
