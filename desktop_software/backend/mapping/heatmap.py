"""
IDW (Inverse Distance Weighting) heatmap interpolation.

Interpolates anomaly intensity onto a regular lat/lon grid from sparse
deployment-point evidence.

UI label: "Interpolated anomaly intensity"
Disclaimer: "Values are interpolated estimates - NOT direct measurements."

All output points tagged DataProvenance.INFERRED (interpolated = inferred).
"""

from __future__ import annotations

import logging
from typing import List, Optional, Tuple

import numpy as np

from backend.models.schemas import ClassificationResult, DataProvenance, HeatmapPoint

log = logging.getLogger(__name__)


def _idw(
    known_lats: np.ndarray,
    known_lons: np.ndarray,
    known_vals: np.ndarray,
    query_lats: np.ndarray,
    query_lons: np.ndarray,
    power: float = 2.0,
    epsilon: float = 1e-6,
) -> np.ndarray:
    """
    Inverse Distance Weighting interpolation.

    Parameters
    ----------
    known_*   : arrays of known measurement locations and values
    query_*   : arrays of grid locations to interpolate onto
    power     : IDW exponent (2.0 is standard)
    epsilon   : minimum distance to avoid division by zero

    Returns
    -------
    Interpolated values at query locations
    """
    result = np.zeros(len(query_lats))
    for i in range(len(query_lats)):
        # Approximate distances in degrees (adequate for local prototype grids)
        dlat = known_lats - query_lats[i]
        dlon = known_lons - query_lons[i]
        dist = np.sqrt(dlat ** 2 + dlon ** 2)
        dist = np.maximum(dist, epsilon)

        weights = 1.0 / (dist ** power)
        result[i] = np.sum(weights * known_vals) / np.sum(weights)

    return result


def build_heatmap(
    classifications: List[ClassificationResult],
    sectors: list,  # List[DriftSector] — station centres
    anomaly_events: list = None,
    grid_steps: int = 40,
    power: float = 2.0,
) -> List[HeatmapPoint]:
    """
    Build an IDW-interpolated heatmap grid of evidence_score.

    Parameters
    ----------
    classifications : list of per-station ClassificationResult
    sectors         : list of DriftSector (provides lat/lon of each station)
    grid_steps      : number of grid cells per axis (grid_steps x grid_steps total)
    power           : IDW exponent

    Returns
    -------
    List of HeatmapPoint (one per grid cell)
    """
    if not classifications or not sectors:
        log.warning("No data for heatmap")
        return []

    # Prefer localized anomaly observations; station centres are only a fallback.
    if anomaly_events:
        known_lats = np.array([e.latitude for e in anomaly_events], dtype=float)
        known_lons = np.array([e.longitude for e in anomaly_events], dtype=float)
        known_vals = np.array([e.evidence_score for e in anomaly_events], dtype=float)
    else:
        known_lats, known_lons, known_vals = [], [], []

    # Map station_id -> lat/lon for legacy/fallback inputs.
    sector_map = {s.station_id: (s.deploy_latitude, s.deploy_longitude) for s in sectors}

    if not anomaly_events:
        for cls in classifications:
            pos = sector_map.get(cls.station_id)
            if pos is not None:
                known_lats.append(pos[0]); known_lons.append(pos[1]); known_vals.append(cls.evidence_score)

    if len(known_lats) < 1:
        log.warning("No station positions available for heatmap")
        return []

    known_lats = np.array(known_lats)
    known_lons = np.array(known_lons)
    known_vals = np.array(known_vals)

    # Build grid bounds (add 20% padding around known points)
    pad_lat = max((known_lats.max() - known_lats.min()) * 0.20, 0.005)
    pad_lon = max((known_lons.max() - known_lons.min()) * 0.20, 0.005)

    lat_min = known_lats.min() - pad_lat
    lat_max = known_lats.max() + pad_lat
    lon_min = known_lons.min() - pad_lon
    lon_max = known_lons.max() + pad_lon

    grid_lats = np.linspace(lat_min, lat_max, grid_steps)
    grid_lons = np.linspace(lon_min, lon_max, grid_steps)

    q_lats, q_lons = np.meshgrid(grid_lats, grid_lons)
    q_lats_flat = q_lats.ravel()
    q_lons_flat = q_lons.ravel()

    if len(known_lats) == 1:
        # Single station: constant field
        interpolated = np.full(len(q_lats_flat), float(known_vals[0]))
    else:
        interpolated = _idw(known_lats, known_lons, known_vals,
                            q_lats_flat, q_lons_flat, power=power)

    points: List[HeatmapPoint] = []
    for lat, lon, val in zip(q_lats_flat, q_lons_flat, interpolated):
        points.append(HeatmapPoint(
            lat=float(lat),
            lon=float(lon),
            value=float(np.clip(val, 0.0, 1.0)),
            data_provenance=DataProvenance.INFERRED,
        ))

    log.info("Heatmap: %d grid points (%dx%d), power=%.1f", len(points), grid_steps, grid_steps, power)
    return points
