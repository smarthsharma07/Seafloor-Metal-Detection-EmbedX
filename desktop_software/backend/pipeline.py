"""
Processing pipeline orchestrator.

Runs the full end-to-end pipeline for one or more deployments:
  1. Read telemetry (CSV or JSON)
  2. Validate
  3. Preprocess
  4. Extract features
  5. Detect anomalies
  6. Classify
  7. Build drift sectors
  8. Find multi-station candidates
  9. Build IDW heatmap
  10. Export GeoJSON

Raw data is preserved throughout — no overwriting.
"""

from __future__ import annotations

import logging
from datetime import datetime, timezone
from pathlib import Path
from typing import Dict, List, Optional, Tuple

import pandas as pd
import numpy as np

from backend.ingestion.reader import read_telemetry
from backend.ingestion.validator import validate
from backend.preprocessing.cleaner import preprocess
from backend.features.extractor import extract_features
from backend.detection.anomaly_detector import run_all_detectors
from backend.classification.rule_based import classify
from backend.localization.intersection import build_drift_sector, find_candidates
from backend.mapping.heatmap import build_heatmap
from backend.reports.geojson_exporter import export_geojson
from backend.models.schemas import (
    ClassificationResult,
    AnomalyEvent,
    AnomalyType,
    DataProvenance,
    DeploymentMetadata,
    DriftSector,
    ExportBundle,
    HeatmapPoint,
    MultiStationCandidate,
    ValidationReport,
)

log = logging.getLogger(__name__)


class PipelineResult:
    """Container for all pipeline outputs."""

    def __init__(self):
        self.stations:            List[DeploymentMetadata]    = []
        self.dataframes:          Dict[str, pd.DataFrame]     = {}
        self.validation_reports:  List[ValidationReport]      = []
        self.classifications:     List[ClassificationResult]  = []
        self.anomaly_events:      List[AnomalyEvent]            = []
        self.drift_sectors:       List[DriftSector]           = []
        self.candidates:          List[MultiStationCandidate] = []
        self.heatmap_points:      List[HeatmapPoint]          = []

    def to_export_bundle(self) -> ExportBundle:
        return ExportBundle(
            generated_at=datetime.now(timezone.utc).isoformat(),
            stations=self.stations,
            validation_reports=self.validation_reports,
            classifications=self.classifications,
            anomaly_events=self.anomaly_events,
            drift_sectors=self.drift_sectors,
            candidates=self.candidates,
            heatmap_points=self.heatmap_points,
        )


def _extract_drift_bearing(df: pd.DataFrame) -> Tuple[Optional[float], Optional[float]]:
    """
    Extract mean drift bearing and distance from telemetry.
    Returns (bearing_deg, distance_m) or (None, None).
    """
    if "drift_north_m" in df.columns and "drift_east_m" in df.columns:
        import numpy as np
        north = pd.to_numeric(df["drift_north_m"], errors="coerce").dropna()
        east  = pd.to_numeric(df["drift_east_m"],  errors="coerce").dropna()
        if len(north) > 0 and len(east) > 0:
            mean_n = float(north.mean())
            mean_e = float(east.mean())
            dist   = float(np.sqrt(mean_n ** 2 + mean_e ** 2))
            bearing = float(np.degrees(np.arctan2(mean_e, mean_n))) % 360.0
            return bearing, dist
    if "heading_deg" in df.columns:
        heading = pd.to_numeric(df["heading_deg"], errors="coerce").dropna()
        if len(heading) > 0:
            return float(heading.mean()), None
    return None, None


def _extract_anomaly_events(df: pd.DataFrame, meta: DeploymentMetadata) -> List[AnomalyEvent]:
    """Extract contiguous local excursions while preserving their locations."""
    if df.empty or "timestamp_rtc" not in df.columns:
        return []
    temp = pd.to_numeric(df.get("water_temp_c"), errors="coerce")
    pi = pd.to_numeric(df.get("pi_anomaly_strength"), errors="coerce")
    mag = pd.to_numeric(df.get("mag_total_ut"), errors="coerce")
    temp_delta = temp - temp.rolling(31, min_periods=8).median().shift(1).bfill().ffill()
    pi_delta = pi - pi.rolling(31, min_periods=8).median().shift(1).bfill().ffill()
    mag_delta = (mag - mag.rolling(31, min_periods=8).median().shift(1).bfill().ffill()).abs()
    flags = pd.DataFrame({"pi": pi_delta > 0.12, "thermal": temp_delta > 0.35, "mag": mag_delta > 2.0}, index=df.index).fillna(False)
    active = flags.any(axis=1)
    groups = (active != active.shift(fill_value=False)).cumsum()
    events: List[AnomalyEvent] = []
    for _, idx in active[active].groupby(groups[active]).groups.items():
        rows = df.loc[idx]
        present = flags.loc[idx].any()
        rep_idx = idx[len(idx) // 2]
        row = df.loc[rep_idx]
        has_pi, has_temp, has_mag = bool(present.pi), bool(present.thermal), bool(present.mag)
        if has_pi and has_temp:
            classification = AnomalyType.POSSIBLE_HYDROTHERMAL_ASSOCIATED_ANOMALY
        elif has_pi or has_mag:
            classification = AnomalyType.POSSIBLE_METALLIC_ANOMALY
        elif has_temp:
            classification = AnomalyType.POSSIBLE_THERMAL_ANOMALY
        else:
            classification = AnomalyType.INSUFFICIENT_DATA
        lat = row.get("latitude")
        lon = row.get("longitude")
        lat = float(lat) if pd.notna(lat) else meta.deploy_latitude
        lon = float(lon) if pd.notna(lon) else meta.deploy_longitude
        score = min(1.0, 0.15 + 0.35 * has_pi + 0.25 * has_temp + 0.25 * has_mag)
        events.append(AnomalyEvent(
            station_id=meta.station_id,
            start_timestamp=str(rows.iloc[0]["timestamp_rtc"]),
            end_timestamp=str(rows.iloc[-1]["timestamp_rtc"]),
            representative_timestamp=str(row["timestamp_rtc"]),
            latitude=lat, longitude=lon,
            pi_strength=float(row["pi_anomaly_strength"]) if pd.notna(row.get("pi_anomaly_strength")) else None,
            temperature=float(row["water_temp_c"]) if pd.notna(row.get("water_temp_c")) else None,
            temperature_delta=float(temp_delta.loc[rep_idx]) if pd.notna(temp_delta.loc[rep_idx]) else None,
            magnetic_strength=float(row["mag_total_ut"]) if pd.notna(row.get("mag_total_ut")) else None,
            heading=float(row["heading_deg"]) if pd.notna(row.get("heading_deg")) else None,
            evidence_score=score, classification=classification,
            evidence_flags=[name for name, ok in (("PI_LOCAL_EXCURSION", has_pi), ("TEMPERATURE_LOCAL_EXCURSION", has_temp), ("MAGNETIC_LOCAL_EXCURSION", has_mag)) if ok],
            data_provenance=DataProvenance.DEMO if meta.demo_mode else DataProvenance.INFERRED,
        ))
    return events


def run_pipeline(
    inputs: List[Tuple[pd.DataFrame, DeploymentMetadata]],
    output_dir: Optional[Path] = None,
) -> PipelineResult:
    """
    Run the full processing pipeline.

    Parameters
    ----------
    inputs     : list of (raw_dataframe, DeploymentMetadata) tuples
    output_dir : directory to write GeoJSON export (optional)

    Returns
    -------
    PipelineResult
    """
    result = PipelineResult()

    for df_raw, meta in inputs:
        sid = meta.station_id
        log.info("=== Pipeline: station %s ===", sid)

        result.stations.append(meta)

        # Step 1: Validate
        report = validate(df_raw, sid)
        result.validation_reports.append(report)

        # Step 2: Preprocess (returns new df, raw unchanged)
        df_proc = preprocess(df_raw.copy(), sid)
        result.dataframes[sid] = df_proc
        result.anomaly_events.extend(_extract_anomaly_events(df_proc, meta))

        # Step 3: Feature extraction
        features = extract_features(df_proc, sid)

        # Step 4: Anomaly detection
        flags = run_all_detectors(features, sid)

        # Step 5: Classification
        cls_result = classify(flags, features, sid)
        station_events = [e for e in result.anomaly_events if e.station_id == sid]
        if station_events:
            strongest = max(station_events, key=lambda e: e.evidence_score)
            cls_result = ClassificationResult(
                station_id=sid,
                anomaly_type=strongest.classification,
                evidence_score=max(e.evidence_score for e in station_events),
                contributing_channels=strongest.evidence_flags,
                rationale="Localized anomaly windows: " + "; ".join(str(e) for e in strongest.evidence_flags),
                data_provenance=DataProvenance.DEMO if meta.demo_mode else DataProvenance.INFERRED,
            )
        result.classifications.append(cls_result)

        # Step 6: Drift sector
        bearing, distance = _extract_drift_bearing(df_proc)
        sector = build_drift_sector(meta, bearing, distance)
        result.drift_sectors.append(sector)

    # Step 7: Multi-station candidates
    result.candidates = find_candidates(result.drift_sectors, result.classifications)

    # Step 8: IDW heatmap
    result.heatmap_points = build_heatmap(
        result.classifications,
        result.drift_sectors,
        result.anomaly_events,
    )

    # Step 9: GeoJSON export
    if output_dir is not None:
        output_dir = Path(output_dir)
        output_dir.mkdir(parents=True, exist_ok=True)
        ts = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
        geojson_path = output_dir / f"results_{ts}.geojson"
        export_geojson(
            path=geojson_path,
            stations=result.stations,
            classifications=result.classifications,
            anomaly_events=result.anomaly_events,
            sectors=result.drift_sectors,
            heatmap_points=result.heatmap_points,
            candidates=result.candidates,
        )
        log.info("Results exported to %s", geojson_path)

    return result


def run_pipeline_from_files(
    file_paths: List[Path],
    metadata_list: List[DeploymentMetadata],
    output_dir: Optional[Path] = None,
) -> PipelineResult:
    """
    Convenience wrapper: read telemetry files then run pipeline.
    """
    inputs = []
    for path, meta in zip(file_paths, metadata_list):
        df = read_telemetry(path)
        inputs.append((df, meta))
    return run_pipeline(inputs, output_dir=output_dir)
