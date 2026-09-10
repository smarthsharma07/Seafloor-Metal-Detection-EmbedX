from __future__ import annotations

import sys
from pathlib import Path
from types import SimpleNamespace


ROOT = Path(__file__).resolve().parents[1] / "desktop_software"
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from backend.classification.rule_based import classify
from backend.detection.anomaly_detector import THRESHOLDS
from backend.localization.intersection import build_drift_sector, compute_candidate_score, find_candidates
from backend.mapping.heatmap import build_heatmap
from backend.models.schemas import (
    AnomalyFlag,
    AnomalyType,
    DataProvenance,
    DeploymentMetadata,
    SensorFeatures,
)
from backend.reports.geojson_exporter import build_geojson


def _feature(channel: str, mean: float, n_valid: int = 100, n_samples: int = 100) -> SensorFeatures:
    return SensorFeatures(
        station_id="STATION_A",
        channel=channel,
        n_samples=n_samples,
        n_valid=n_valid,
        mean=mean,
        std=0.1,
        min_val=mean - 0.1,
        max_val=mean + 0.1,
        p5=mean - 0.05,
        p95=mean + 0.05,
        data_provenance=DataProvenance.CALCULATED,
    )


def test_classification_uses_heuristic_evidence_score():
    flags = [
        AnomalyFlag(
            station_id="STATION_A",
            channel="pi_anomaly_strength",
            detector="pi_mean_threshold",
            is_anomalous=True,
            trigger_value=0.35,
            threshold_used=THRESHOLDS["pi_anomaly_strength_mean"],
            description="PI anomaly",
            data_provenance=DataProvenance.INFERRED,
        ),
        AnomalyFlag(
            station_id="STATION_A",
            channel="water_temp_c",
            detector="temp_range_threshold",
            is_anomalous=True,
            trigger_value=2.8,
            threshold_used=THRESHOLDS["water_temp_range_c"],
            description="Thermal anomaly",
            data_provenance=DataProvenance.INFERRED,
        ),
    ]
    features = [
        _feature("pi_anomaly_strength", 0.35),
        _feature("water_temp_c", 6.2),
        _feature("mag_total_ut", 42.0),
    ]

    result = classify(flags, features, "STATION_A")

    assert result.data_provenance is DataProvenance.INFERRED
    assert result.evidence_score > 0
    assert result.evidence_score <= 1
    assert result.anomaly_type in {
        AnomalyType.MULTISENSOR_ANOMALY,
        AnomalyType.POSSIBLE_THERMAL_ANOMALY,
        AnomalyType.POSSIBLE_METALLIC_ANOMALY,
        AnomalyType.POSSIBLE_HYDROTHERMAL_ASSOCIATED_ANOMALY,
    }


def test_candidate_score_is_documented_heuristic():
    score = compute_candidate_score({"A": 0.7, "B": 0.5}, overlap_fraction=0.6, data_quality=0.8)
    assert 0 <= score <= 1
    expected = (0.50 * 0.6) + (0.30 * 0.6) + (0.20 * 0.8)
    assert abs(score - expected) <= 1e-4


def test_overlap_regions_and_heatmap_labels_are_explicit():
    meta_a = DeploymentMetadata(
        station_id="A",
        deploy_latitude=14.9800,
        deploy_longitude=74.0100,
        drift_scale_m=400.0,
    )
    meta_b = DeploymentMetadata(
        station_id="B",
        deploy_latitude=14.9800,
        deploy_longitude=74.0100,
        drift_scale_m=400.0,
    )

    sec_a = build_drift_sector(meta_a, 90.0, 200.0)
    sec_b = build_drift_sector(meta_b, 100.0, 200.0)
    cls_a = SimpleNamespace(
        station_id="A",
        evidence_score=0.8,
        anomaly_type=AnomalyType.MULTISENSOR_ANOMALY,
        contributing_channels=["pi_anomaly_strength", "water_temp_c"],
        rationale="synthetic demo",
        data_provenance=DataProvenance.INFERRED,
    )
    cls_b = SimpleNamespace(
        station_id="B",
        evidence_score=0.7,
        anomaly_type=AnomalyType.POSSIBLE_METALLIC_ANOMALY,
        contributing_channels=["pi_anomaly_strength"],
        rationale="synthetic demo",
        data_provenance=DataProvenance.INFERRED,
    )

    candidates = find_candidates([sec_a, sec_b], [cls_a, cls_b])
    assert candidates
    assert candidates[0].overlap_geojson is not None
    assert candidates[0].data_provenance is DataProvenance.INFERRED

    heatmap = build_heatmap(
        [type("R", (), {"station_id": "A", "evidence_score": 0.8})(), type("R", (), {"station_id": "B", "evidence_score": 0.7})()],
        [sec_a, sec_b],
        grid_steps=5,
    )
    assert heatmap
    assert all(point.data_provenance is DataProvenance.INFERRED for point in heatmap)

    geojson = build_geojson(
        stations=[meta_a, meta_b],
        classifications=[cls_a, cls_b],
        sectors=[sec_a, sec_b],
        heatmap_points=heatmap,
        candidates=candidates,
    )
    layers = {feature["properties"]["layer"] for feature in geojson["features"]}
    assert "interpolated_heatmap" in layers
    assert "candidate_overlap_regions" in layers
    assert any(
        feature["properties"].get("label") == "Interpolated anomaly intensity"
        for feature in geojson["features"]
        if feature["properties"]["layer"] == "interpolated_heatmap"
    )
