"""
FastAPI application — MoES Desktop Software
SIH 2026 Prototype

Endpoints:
  GET  /                    → Dashboard HTML page
  GET  /api/status          → Server health
  POST /api/upload          → Upload ESP32 CSV/JSON telemetry
  POST /api/run_demo        → Run synthetic DEMO pipeline
  GET  /api/results         → Latest pipeline results (GeoJSON bundle)
  GET  /api/results/raw     → Raw ExportBundle JSON
  GET  /api/telemetry/{sid} → Full telemetry DataFrame for a station
  GET  /api/config          → Current detection thresholds
  POST /api/config          → Update detection thresholds
"""

from __future__ import annotations

import io
import json
import logging
import os
import tempfile
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Dict, List, Optional

import pandas as pd
from fastapi import FastAPI, File, Form, HTTPException, UploadFile
from fastapi.responses import HTMLResponse, JSONResponse
from fastapi.staticfiles import StaticFiles
from fastapi.templating import Jinja2Templates
from fastapi.requests import Request

from backend.pipeline import run_pipeline, PipelineResult
from backend.demo.generator import generate_all_demo_stations
from backend.detection.anomaly_detector import THRESHOLDS
from backend.models.schemas import DeploymentMetadata, DataProvenance
from backend.reports.geojson_exporter import build_geojson

log = logging.getLogger(__name__)

# ---------------------------------------------------------------------------
# App setup
# ---------------------------------------------------------------------------

BASE_DIR = Path(__file__).parent

app = FastAPI(
    title="MoES Underwater Sensor System — Desktop Software",
    description="SIH 2026 Prototype. Evidence / Confidence Score is heuristic and not statistically calibrated.",
    version="1.0.0",
)

# Mount static files
static_dir = BASE_DIR / "frontend" / "static"
static_dir.mkdir(parents=True, exist_ok=True)
app.mount("/static", StaticFiles(directory=str(static_dir)), name="static")

templates = Jinja2Templates(directory=str(BASE_DIR / "frontend" / "templates"))

# ---------------------------------------------------------------------------
# In-memory session state (prototype; not persistent across restarts)
# ---------------------------------------------------------------------------

_session: Dict[str, Any] = {
    "pipeline_result": None,
    "raw_dataframes":  {},   # station_id -> DataFrame
    "thresholds":      dict(THRESHOLDS),
}


# ---------------------------------------------------------------------------
# Routes
# ---------------------------------------------------------------------------

@app.get("/", response_class=HTMLResponse)
async def dashboard(request: Request):
    """Serve the main GIS dashboard."""
    return templates.TemplateResponse("dashboard.html", {"request": request})


@app.get("/api/status")
async def status():
    return {
        "status":       "ok",
        "timestamp":    datetime.now(timezone.utc).isoformat(),
        "has_results":  _session["pipeline_result"] is not None,
        "version":      "1.0.0",
    }


@app.post("/api/upload")
async def upload_telemetry(
    file: UploadFile = File(...),
    station_id: str = Form(...),
    latitude:   float = Form(...),
    longitude:  float = Form(...),
    depth_m:    Optional[float] = Form(None),
    drift_scale_m: float = Form(400.0),
    notes:      Optional[str] = Form(None),
):
    """
    Upload one telemetry file and run the processing pipeline.

    file       : CSV or JSON telemetry from ESP32
    station_id : unique identifier for this deployment
    latitude   : deployment GPS latitude
    longitude  : deployment GPS longitude
    depth_m    : deployment depth (optional)
    drift_scale_m : uncertainty radius in metres (default 400)
    notes      : operator notes
    """
    # Read uploaded file bytes
    contents = await file.read()
    suffix = Path(file.filename).suffix.lower() if file.filename else ".csv"

    with tempfile.NamedTemporaryFile(delete=False, suffix=suffix) as tmp:
        tmp.write(contents)
        tmp_path = Path(tmp.name)

    try:
        from backend.ingestion.reader import read_telemetry
        df = read_telemetry(tmp_path)
    finally:
        tmp_path.unlink(missing_ok=True)

    meta = DeploymentMetadata(
        station_id=station_id,
        deploy_latitude=latitude,
        deploy_longitude=longitude,
        deploy_depth_m=depth_m,
        drift_scale_m=drift_scale_m,
        operator_notes=notes,
        demo_mode=False,
        data_provenance=DataProvenance.MEASURED,
    )

    # If existing session has other stations, keep them
    existing_inputs = _get_existing_inputs()
    existing_inputs.append((df, meta))
    _session["raw_dataframes"][station_id] = df

    # Re-run pipeline with all stations
    result = run_pipeline(existing_inputs, output_dir=BASE_DIR / "data" / "exports")
    _session["pipeline_result"] = result

    # Apply current threshold configuration
    _apply_thresholds()

    return {
        "status":        "ok",
        "station_id":    station_id,
        "rows_loaded":   len(df),
        "n_stations":    len(result.stations),
        "classification": result.classifications[-1].dict() if result.classifications else None,
    }


@app.post("/api/run_demo")
async def run_demo():
    """
    Run the synthetic DEMO pipeline with 3 pre-configured stations.
    All results are tagged DataProvenance.DEMO.
    """
    inputs = generate_all_demo_stations()
    result = run_pipeline(inputs, output_dir=BASE_DIR / "data" / "exports")
    _session["pipeline_result"] = result
    _session["raw_dataframes"] = {meta.station_id: df for df, meta in inputs}

    return {
        "status":      "ok",
        "demo_mode":   True,
        "n_stations":  len(result.stations),
        "n_candidates": len(result.candidates),
        "disclaimer":  "DEMO data — synthetic, NOT real measurements",
        "classifications": [c.dict() for c in result.classifications],
    }


@app.get("/api/results")
async def get_results_geojson():
    """Return the latest pipeline results as a GeoJSON FeatureCollection."""
    result: Optional[PipelineResult] = _session["pipeline_result"]
    if result is None:
        raise HTTPException(status_code=404, detail="No results yet. Upload data or run demo.")

    geojson = build_geojson(
        stations=result.stations,
        classifications=result.classifications,
        anomaly_events=result.anomaly_events,
        sectors=result.drift_sectors,
        heatmap_points=result.heatmap_points,
        candidates=result.candidates,
    )
    return JSONResponse(content=geojson)


@app.get("/api/results/raw")
async def get_results_raw():
    """Return the full ExportBundle as JSON."""
    result: Optional[PipelineResult] = _session["pipeline_result"]
    if result is None:
        raise HTTPException(status_code=404, detail="No results yet.")
    bundle = result.to_export_bundle()
    return bundle.dict()


@app.get("/api/telemetry/{station_id}")
async def get_telemetry(station_id: str, max_rows: int = 500):
    """
    Return processed telemetry for a station as JSON records.
    max_rows limits output size (default 500).
    """
    df = _session["raw_dataframes"].get(station_id)
    if df is None:
        raise HTTPException(status_code=404, detail=f"Station '{station_id}' not found.")
    # Return a downsampled slice to keep response size manageable
    step = max(1, len(df) // max_rows)
    df_out = df.iloc[::step].copy()
    # Convert timestamps to strings for JSON serialisation
    for col in df_out.columns:
        if hasattr(df_out[col], "dt"):
            df_out[col] = df_out[col].astype(str)
    records = df_out.where(pd.notnull(df_out), None).to_dict(orient="records")
    return {"station_id": station_id, "n_rows": len(records), "records": records}


@app.get("/api/config")
async def get_config():
    """Return current detection threshold configuration."""
    return {
        "thresholds": _session["thresholds"],
        "note": "These are heuristic thresholds — adjust based on field experience.",
    }


@app.post("/api/config")
async def set_config(body: Dict[str, float]):
    """
    Update one or more detection thresholds.

    Send a JSON body with threshold key-value pairs, e.g.:
      { "pi_anomaly_strength_mean": 0.20 }
    """
    from backend.detection.anomaly_detector import THRESHOLDS as _T
    updated = []
    for key, value in body.items():
        if key in _T:
            _T[key] = float(value)
            _session["thresholds"][key] = float(value)
            updated.append(key)
        else:
            log.warning("Unknown threshold key: %s", key)
    return {"status": "ok", "updated": updated, "thresholds": _session["thresholds"]}


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _get_existing_inputs():
    """Reconstruct inputs list from session state."""
    result: Optional[PipelineResult] = _session["pipeline_result"]
    if result is None:
        return []
    inputs = []
    for meta in result.stations:
        df = _session["raw_dataframes"].get(meta.station_id)
        if df is not None:
            inputs.append((df, meta))
    return inputs


def _apply_thresholds():
    """Re-apply any custom threshold values to the detector module."""
    from backend.detection import anomaly_detector
    for k, v in _session["thresholds"].items():
        if k in anomaly_detector.THRESHOLDS:
            anomaly_detector.THRESHOLDS[k] = v
