"""
GeoJSON exporter.

Produces a FeatureCollection with visually distinct layers:
  - "deployment_points"    : deployment station markers
  - "measured_anomaly_points" : station anomaly markers derived from measured telemetry
  - "drift_sectors"        : uncertainty sector polygons (CALCULATED)
  - "interpolated_heatmap"  : IDW interpolated intensity grid (INFERRED)
  - "candidate_overlap_regions" : multi-station overlap polygons (INFERRED)

Each feature includes a `data_provenance` property.
"""

from __future__ import annotations

import json
import logging
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Dict, List

from backend.models.schemas import (
    ClassificationResult,
    AnomalyEvent,
    DeploymentMetadata,
    DriftSector,
    HeatmapPoint,
    MultiStationCandidate,
)

log = logging.getLogger(__name__)


def _feature(geometry: Dict, properties: Dict) -> Dict:
    return {"type": "Feature", "geometry": geometry, "properties": properties}


def _point(lon: float, lat: float) -> Dict:
    return {"type": "Point", "coordinates": [lon, lat]}


def build_geojson(
    stations: List[DeploymentMetadata],
    classifications: List[ClassificationResult],
    sectors: List[DriftSector],
    heatmap_points: List[HeatmapPoint],
    candidates: List[MultiStationCandidate],
    anomaly_events: List[AnomalyEvent] = None,
) -> Dict[str, Any]:
    """
    Build a GeoJSON FeatureCollection with all map layers.

    Returns a dict (JSON-serialisable).
    """
    features: List[Dict] = []
    class_map = {c.station_id: c for c in classifications}

    # ---- Layer 1: deployment points ------------------------------------
    for st in stations:
        cls = class_map.get(st.station_id)
        features.append(_feature(
            geometry=_point(st.deploy_longitude, st.deploy_latitude),
            properties={
                "layer":           "deployment_points",
                "station_id":      st.station_id,
                "depth_m":         st.deploy_depth_m,
                "drift_scale_m":   st.drift_scale_m,
                "demo_mode":       st.demo_mode,
                "data_provenance": st.data_provenance.value,
                "notes":           st.operator_notes or "",
            },
        ))

    # ---- Layer 2: localized measured/calculated anomaly observations ----
    for event in anomaly_events or []:
        features.append(_feature(
            geometry=_point(event.longitude, event.latitude),
            properties={"layer": "measured_anomaly_points", **event.model_dump(mode="json"), "anomaly_type": event.classification.value},
        ))

    # ---- Station summary classification point ---------------------------
    for cls in classifications:
        st = next((s for s in stations if s.station_id == cls.station_id), None)
        if st is None:
            continue
        features.append(_feature(
            geometry=_point(st.deploy_longitude, st.deploy_latitude),
            properties={
                "layer":               "station_classification_summary",
                "station_id":          cls.station_id,
                "anomaly_type":        cls.anomaly_type.value,
                "evidence_score":      cls.evidence_score,
                "evidence_label":      "Evidence / Confidence Score",
                "evidence_disclaimer": "Rule-based, not statistically calibrated.",
                "contributing_channels": cls.contributing_channels,
                "rationale":           cls.rationale,
                "data_provenance":     cls.data_provenance.value,
            },
        ))

    # ---- Layer 3: drift sector polygons --------------------------------
    for sec in sectors:
        if sec.sector_geojson is None:
            continue
        features.append(_feature(
            geometry=sec.sector_geojson,
            properties={
                "layer":           "drift_sectors",
                "station_id":      sec.station_id,
                "bearing_deg":     sec.drift_bearing_deg,
                "drift_radius_m":  sec.drift_scale_m,
                "note":            "Uncertainty extent — NOT exact target position",
                "data_provenance": sec.data_provenance.value,
            },
        ))

    # ---- Layer 4: IDW heatmap grid (points) ----------------------------
    for hp in heatmap_points:
        if hp.value < 0.05:
            continue  # skip near-zero points for file size
        features.append(_feature(
            geometry=_point(hp.lon, hp.lat),
            properties={
                "layer":           "interpolated_heatmap",
                "value":           round(hp.value, 4),
                "label":           "Interpolated anomaly intensity",
                "disclaimer":      "Interpolated estimate — NOT a direct measurement",
                "data_provenance": hp.data_provenance.value,
            },
        ))

    # ---- Layer 5: multi-station candidate overlap regions -------------
    for cand in candidates:
        geometry = cand.overlap_geojson or _point(cand.centroid_lon, cand.centroid_lat)
        features.append(_feature(
            geometry=geometry,
            properties={
                "layer":               "candidate_overlap_regions",
                "candidate_id":        cand.candidate_id,
                "station_ids":         cand.station_ids,
                "classification":      cand.classification.value,
                "candidate_score":     cand.candidate_score,
                "score_label":         "Candidate score",
                "score_disclaimer":    "Documented heuristic based on station evidence, spatial overlap, and data quality",
                "overlap_area_m2":     cand.overlap_area_m2,
                "station_scores":      cand.station_scores,
                "data_provenance":     cand.data_provenance.value,
            },
        ))

    return {
        "type":     "FeatureCollection",
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "features": features,
    }


def export_geojson(path: Path | str, **kwargs) -> Path:
    """Write GeoJSON to file and return the path."""
    path = Path(path)
    geojson = build_geojson(**kwargs)
    path.write_text(json.dumps(geojson, indent=2), encoding="utf-8")
    log.info("GeoJSON exported: %s (%d features)", path, len(geojson["features"]))
    return path
