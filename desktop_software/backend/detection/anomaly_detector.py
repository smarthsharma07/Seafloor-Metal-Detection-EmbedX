"""
Anomaly detection: rule-based threshold detectors.

Each detector inspects one sensor channel and returns an AnomalyFlag.
All outputs tagged DataProvenance.INFERRED.

Thresholds are configurable via the THRESHOLDS dict.
No machine-learning is used.
"""

from __future__ import annotations

import logging
from typing import Dict, List, Optional

import numpy as np
import pandas as pd

from backend.models.schemas import AnomalyFlag, DataProvenance, SensorFeatures

log = logging.getLogger(__name__)

# ---------------------------------------------------------------------------
# Configurable thresholds
# These are heuristic starting points; operators should tune via /config.
# ---------------------------------------------------------------------------

THRESHOLDS: Dict[str, float] = {
    # PI anomaly strength: flag if mean > threshold
    "pi_anomaly_strength_mean":     0.15,
    # PI anomaly strength: flag if p95 > threshold
    "pi_anomaly_strength_p95":      0.30,
    # Magnetic total field: flag if std/mean ratio > threshold (relative variation)
    "mag_total_relative_std":       0.05,
    # Water temperature anomaly: flag if range (max-min) > threshold °C
    "water_temp_range_c":           2.0,
    # Water temperature anomaly: flag if mean deviates from 4°C deep-water reference by > threshold
    # (4°C is a rough reference — adjust per deployment region)
    "water_temp_abs_deviation_c":   8.0,
    # Drift radius: flag if mean > threshold (sensor may be moving a lot)
    "drift_radius_mean_m":          300.0,
}


# ---------------------------------------------------------------------------
# Individual detectors
# ---------------------------------------------------------------------------

def _flag(station_id: str, channel: str, detector: str,
          is_anomalous: bool, trigger: Optional[float],
          threshold: Optional[float], description: str) -> AnomalyFlag:
    return AnomalyFlag(
        station_id=station_id,
        channel=channel,
        detector=detector,
        is_anomalous=is_anomalous,
        trigger_value=trigger,
        threshold_used=threshold,
        description=description,
        data_provenance=DataProvenance.INFERRED,
    )


def detect_pi_anomaly(features: Dict[str, SensorFeatures], station_id: str) -> List[AnomalyFlag]:
    """Pulse-induction anomaly strength detectors."""
    flags: List[AnomalyFlag] = []
    feat = features.get("pi_anomaly_strength")
    if feat is None or feat.n_valid == 0:
        flags.append(_flag(station_id, "pi_anomaly_strength", "pi_mean",
                           False, None, None, "No PI data available"))
        return flags

    thr_mean = THRESHOLDS["pi_anomaly_strength_mean"]
    thr_p95  = THRESHOLDS["pi_anomaly_strength_p95"]

    flags.append(_flag(
        station_id, "pi_anomaly_strength", "pi_mean_threshold",
        is_anomalous=(feat.mean or 0) > thr_mean,
        trigger=feat.mean,
        threshold=thr_mean,
        description=f"PI mean anomaly strength {feat.mean:.4g} vs threshold {thr_mean}",
    ))

    flags.append(_flag(
        station_id, "pi_anomaly_strength", "pi_p95_threshold",
        is_anomalous=(feat.p95 or 0) > thr_p95,
        trigger=feat.p95,
        threshold=thr_p95,
        description=f"PI p95 anomaly strength {feat.p95:.4g} vs threshold {thr_p95}",
    ))

    return flags


def detect_magnetic_anomaly(features: Dict[str, SensorFeatures], station_id: str) -> List[AnomalyFlag]:
    """Magnetic total field relative standard deviation detector."""
    flags: List[AnomalyFlag] = []
    feat = features.get("mag_total_ut")
    if feat is None or feat.n_valid == 0:
        flags.append(_flag(station_id, "mag_total_ut", "mag_rel_std",
                           False, None, None, "No magnetometer data"))
        return flags

    rel_std = (feat.std or 0) / max(abs(feat.mean or 1), 1e-9)
    thr = THRESHOLDS["mag_total_relative_std"]
    flags.append(_flag(
        station_id, "mag_total_ut", "mag_rel_std_threshold",
        is_anomalous=rel_std > thr,
        trigger=rel_std,
        threshold=thr,
        description=f"Magnetic relative std {rel_std:.4g} vs threshold {thr}",
    ))
    return flags


def detect_thermal_anomaly(features: Dict[str, SensorFeatures], station_id: str) -> List[AnomalyFlag]:
    """Water temperature anomaly detectors."""
    flags: List[AnomalyFlag] = []
    feat = features.get("water_temp_c")
    if feat is None or feat.n_valid == 0:
        flags.append(_flag(station_id, "water_temp_c", "temp_range",
                           False, None, None, "No water temperature data"))
        return flags

    # Range check
    temp_range = (feat.max_val or 0) - (feat.min_val or 0)
    thr_range = THRESHOLDS["water_temp_range_c"]
    flags.append(_flag(
        station_id, "water_temp_c", "temp_range_threshold",
        is_anomalous=temp_range > thr_range,
        trigger=temp_range,
        threshold=thr_range,
        description=f"Water temp range {temp_range:.3g}°C vs threshold {thr_range}°C",
    ))

    # Absolute deviation from deep-water reference
    dev = abs((feat.mean or 0) - 4.0)
    thr_dev = THRESHOLDS["water_temp_abs_deviation_c"]
    flags.append(_flag(
        station_id, "water_temp_c", "temp_abs_deviation",
        is_anomalous=dev > thr_dev,
        trigger=dev,
        threshold=thr_dev,
        description=f"Water temp mean {feat.mean:.3g}°C; deviation {dev:.3g}°C from 4°C reference",
    ))

    return flags


def detect_drift_anomaly(features: Dict[str, SensorFeatures], station_id: str) -> List[AnomalyFlag]:
    """Deployment drift / motion detector."""
    flags: List[AnomalyFlag] = []
    feat = features.get("deployment_drift_radius_m")
    if feat is None or feat.n_valid == 0:
        return flags

    thr = THRESHOLDS["drift_radius_mean_m"]
    flags.append(_flag(
        station_id, "deployment_drift_radius_m", "drift_radius_threshold",
        is_anomalous=(feat.mean or 0) > thr,
        trigger=feat.mean,
        threshold=thr,
        description=f"Mean drift radius {feat.mean:.1f}m vs threshold {thr}m",
    ))
    return flags


# ---------------------------------------------------------------------------
# Master detector
# ---------------------------------------------------------------------------

def run_all_detectors(
    features: List[SensorFeatures],
    station_id: str,
) -> List[AnomalyFlag]:
    """
    Run all anomaly detectors against a station's feature set.

    Parameters
    ----------
    features   : list from features.extractor.extract_features()
    station_id : station identifier

    Returns
    -------
    List of AnomalyFlag (one per detector-channel combination)
    """
    feat_map = {f.channel: f for f in features}

    flags: List[AnomalyFlag] = []
    flags.extend(detect_pi_anomaly(feat_map, station_id))
    flags.extend(detect_magnetic_anomaly(feat_map, station_id))
    flags.extend(detect_thermal_anomaly(feat_map, station_id))
    flags.extend(detect_drift_anomaly(feat_map, station_id))

    anomalous = [f for f in flags if f.is_anomalous]
    log.info("[%s] %d/%d anomaly flags triggered", station_id, len(anomalous), len(flags))
    return flags
