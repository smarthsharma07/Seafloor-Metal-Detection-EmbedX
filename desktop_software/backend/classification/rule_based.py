"""
Rule-based evidence classifier.

Maps anomaly flags + sensor features -> AnomalyType + evidence_score.

evidence_score:
    Heuristic score in [0, 1].
    Rule-based, NOT a statistically calibrated probability.
    UI label: "Evidence / Confidence Score"
    Disclaimer: "Rule-based, not statistically calibrated."

Classification labels (conservative terminology):
    POSSIBLE_METALLIC_ANOMALY
    POSSIBLE_THERMAL_ANOMALY
    POSSIBLE_HYDROTHERMAL_ASSOCIATED_ANOMALY
    MULTISENSOR_ANOMALY
    INSUFFICIENT_DATA
    NONE

Never outputs a definitive ore or mineral identification.
All outputs tagged DataProvenance.INFERRED.
"""

from __future__ import annotations

import logging
from typing import Dict, List

from backend.models.schemas import (
    AnomalyFlag,
    AnomalyType,
    ClassificationResult,
    DataProvenance,
    SensorFeatures,
)

log = logging.getLogger(__name__)

# ---------------------------------------------------------------------------
# Configurable heuristic scoring
# These values are intentionally editable configuration, not probabilities.
# ---------------------------------------------------------------------------

EVIDENCE_WEIGHTS = {
    # PI
    "pi_mean_threshold":    0.35,
    "pi_p95_threshold":     0.20,
    # Magnetic
    "mag_rel_std_threshold": 0.25,
    # Thermal
    "temp_range_threshold":  0.15,
    "temp_abs_deviation":    0.10,
    # Drift (reduces confidence — sensor may have been moving)
    "drift_radius_threshold": -0.10,  # negative weight: penalises high drift
}

EVIDENCE_CONFIG = {
    "min_anomalous_score": 0.15,
    "multisensor_min_channels": 2,
    "quality_valid_fraction": 0.20,
    "quality_floor_multiplier": 0.50,
    "channel_bonus": 0.03,
}

QUALITY_CHANNELS = ("pi_anomaly_strength", "mag_total_ut", "water_temp_c")


def _quality_fraction(features: Dict[str, SensorFeatures]) -> float:
    valid_fractions: List[float] = []
    for channel in QUALITY_CHANNELS:
        feat = features.get(channel)
        if feat is None or feat.n_samples <= 0:
            valid_fractions.append(0.0)
            continue
        valid_fractions.append(max(0.0, min(1.0, feat.n_valid / feat.n_samples)))
    if not valid_fractions:
        return 0.0
    return sum(valid_fractions) / len(valid_fractions)


def _score_from_flags(flags: List[AnomalyFlag]) -> tuple[float, List[str], List[str]]:
    raw_score = 0.0
    contributing_channels: List[str] = []
    rationale_parts: List[str] = []

    for flag in flags:
        if not flag.is_anomalous:
            continue
        weight = EVIDENCE_WEIGHTS.get(flag.detector, 0.05)
        raw_score += weight
        if flag.channel not in contributing_channels:
            contributing_channels.append(flag.channel)
        if flag.trigger_value is not None:
            rationale_parts.append(f"{flag.detector}[{flag.channel}]={flag.trigger_value:.3g}")
        else:
            rationale_parts.append(flag.detector)

    if len(contributing_channels) > 1:
        raw_score += EVIDENCE_CONFIG["channel_bonus"] * (len(contributing_channels) - 1)

    return raw_score, contributing_channels, rationale_parts


def classify(
    flags: List[AnomalyFlag],
    features: List[SensorFeatures],
    station_id: str,
) -> ClassificationResult:
    """
    Classify anomaly type and compute heuristic evidence_score.

    Parameters
    ----------
    flags      : from detection.anomaly_detector.run_all_detectors()
    features   : from features.extractor.extract_features()
    station_id : station identifier

    Returns
    -------
    ClassificationResult
    """
    feat_map = {f.channel: f for f in features}

    # ---- Compute raw evidence score from configurable heuristic weights --
    raw_score, contributing_channels, rationale_parts = _score_from_flags(flags)
    quality_fraction = _quality_fraction(feat_map)
    quality_multiplier = (
        EVIDENCE_CONFIG["quality_floor_multiplier"]
        + (1.0 - EVIDENCE_CONFIG["quality_floor_multiplier"]) * quality_fraction
    )
    evidence_score = max(0.0, min(1.0, raw_score * quality_multiplier))

    # ---- Determine classification label --------------------------------
    anomaly_type: AnomalyType

    if evidence_score < EVIDENCE_CONFIG["min_anomalous_score"]:
        anomaly_type = AnomalyType.NONE

    else:
        n_channels = len(contributing_channels)
        pi_flagged  = any(f.channel == "pi_anomaly_strength" and f.is_anomalous for f in flags)
        mag_flagged = any(f.channel == "mag_total_ut"         and f.is_anomalous for f in flags)
        temp_flagged = any(f.channel == "water_temp_c"        and f.is_anomalous for f in flags)

        # Multi-sensor: at least two distinct channels flagged
        if n_channels >= EVIDENCE_CONFIG["multisensor_min_channels"] and pi_flagged and mag_flagged and temp_flagged:
            anomaly_type = AnomalyType.POSSIBLE_HYDROTHERMAL_ASSOCIATED_ANOMALY
        elif n_channels >= EVIDENCE_CONFIG["multisensor_min_channels"]:
            anomaly_type = AnomalyType.MULTISENSOR_ANOMALY
        elif pi_flagged or mag_flagged:
            anomaly_type = AnomalyType.POSSIBLE_METALLIC_ANOMALY
        elif temp_flagged:
            anomaly_type = AnomalyType.POSSIBLE_THERMAL_ANOMALY
        else:
            anomaly_type = AnomalyType.INSUFFICIENT_DATA

    # Check data quality — if key channels have too little valid data, downgrade
    for key_ch in ("pi_anomaly_strength", "mag_total_ut"):
        feat = feat_map.get(key_ch)
        if feat and feat.n_samples > 0:
            valid_pct = feat.n_valid / feat.n_samples
            if valid_pct < EVIDENCE_CONFIG["quality_valid_fraction"]:
                evidence_score *= EVIDENCE_CONFIG["quality_floor_multiplier"]
                rationale_parts.append(f"low_quality({key_ch}:{valid_pct:.0%})")
                if anomaly_type not in (AnomalyType.NONE, AnomalyType.INSUFFICIENT_DATA):
                    anomaly_type = AnomalyType.INSUFFICIENT_DATA

    rationale = "; ".join(rationale_parts) if rationale_parts else "No anomaly detectors triggered"
    evidence_score = max(0.0, min(1.0, round(evidence_score, 4)))

    log.info(
        "[%s] Classification: %s  evidence_score=%.3f  channels=%s",
        station_id, anomaly_type.value, evidence_score, contributing_channels,
    )

    return ClassificationResult(
        station_id=station_id,
        anomaly_type=anomaly_type,
        evidence_score=evidence_score,
        contributing_channels=contributing_channels,
        rationale=rationale,
        data_provenance=DataProvenance.INFERRED,
    )
