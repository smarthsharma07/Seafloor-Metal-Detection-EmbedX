"""
Geodesic localisation module.

Computes:
  1. Drift vector from deployment point -> estimated target direction
  2. Uncertainty sector polygon (Shapely geometry -> GeoJSON)
  3. Multi-station sector intersection -> candidate overlap region

Key assumptions:
  - drift_scale_m (default 400 m) is the UNCERTAINTY EXTENT, NOT the exact target distance.
  - WGS-84 geodesic calculations via PyProj.
  - Shapely polygons for sector generation and intersection.
  - candidate_score is a DOCUMENTED HEURISTIC based on station evidence,
    spatial overlap, and data quality.

All outputs tagged DataProvenance.CALCULATED or DataProvenance.INFERRED.
"""

from __future__ import annotations

import logging
import math
from typing import Dict, List, Optional, Tuple

import numpy as np

try:
    from pyproj import Geod
    _geod = Geod(ellps="WGS84")
    HAS_PYPROJ = True
except ImportError:
    HAS_PYPROJ = False
    _geod = None

try:
    import shapely.geometry as sg
    import shapely.ops as so
    from pyproj import Transformer
    HAS_SHAPELY = True
except ImportError:
    HAS_SHAPELY = False

_TO_METRIC = Transformer.from_crs("EPSG:4326", "EPSG:32643", always_xy=True) if HAS_SHAPELY else None
_TO_WGS84 = Transformer.from_crs("EPSG:32643", "EPSG:4326", always_xy=True) if HAS_SHAPELY else None

from backend.models.schemas import (
    AnomalyType,
    ClassificationResult,
    DataProvenance,
    DeploymentMetadata,
    DriftSector,
    MultiStationCandidate,
)

log = logging.getLogger(__name__)


# ---------------------------------------------------------------------------
# Geodesic helpers
# ---------------------------------------------------------------------------

def _geodesic_destination(lat: float, lon: float,
                           bearing_deg: float, distance_m: float
                           ) -> Tuple[float, float]:
    """
    Compute destination (lat, lon) given start, bearing, distance.
    Uses PyProj WGS-84 if available; falls back to flat-earth approximation.
    """
    if HAS_PYPROJ:
        lon2, lat2, _ = _geod.fwd(lon, lat, bearing_deg, distance_m)
        return float(lat2), float(lon2)
    else:
        # Flat-earth approximation (acceptable for <10 km)
        R = 6_371_000.0
        lat_r = math.radians(lat)
        dlat = (distance_m / R) * math.cos(math.radians(bearing_deg))
        dlon = (distance_m / (R * math.cos(lat_r))) * math.sin(math.radians(bearing_deg))
        return lat + math.degrees(dlat), lon + math.degrees(dlon)


def _sector_polygon(lat: float, lon: float,
                    bearing_deg: float,
                    half_angle_deg: float,
                    radius_m: float,
                    n_arc_points: int = 20) -> Optional[object]:
    """
    Build a Shapely Polygon representing a circular sector.
    Returns None if Shapely is unavailable.
    """
    if not HAS_SHAPELY:
        return None

    coords = [(lon, lat)]  # sector tip = deployment point
    angles = np.linspace(
        bearing_deg - half_angle_deg,
        bearing_deg + half_angle_deg,
        n_arc_points,
    )
    for ang in angles:
        dest_lat, dest_lon = _geodesic_destination(lat, lon, float(ang), radius_m)
        coords.append((dest_lon, dest_lat))
    coords.append((lon, lat))  # close
    try:
        return sg.Polygon(coords)
    except Exception as exc:
        log.warning("Could not build sector polygon: %s", exc)
        return None


# ---------------------------------------------------------------------------
# Single-station sector
# ---------------------------------------------------------------------------

def build_drift_sector(
    meta: DeploymentMetadata,
    drift_bearing_deg: Optional[float],
    drift_distance_m: Optional[float],
    sector_half_angle_deg: float = 45.0,
) -> DriftSector:
    """
    Build the uncertainty sector for one deployment.

    If no drift bearing is available, returns a full circle at drift_scale_m.
    The sector represents an uncertainty extent, NOT an exact target position.
    """
    lat = meta.deploy_latitude
    lon = meta.deploy_longitude
    radius = meta.drift_scale_m  # use operator-supplied uncertainty extent

    geojson = None

    if drift_bearing_deg is not None and HAS_SHAPELY:
        poly = _sector_polygon(lat, lon, drift_bearing_deg, sector_half_angle_deg, radius)
        if poly:
            geojson = sg.mapping(poly)
    elif HAS_SHAPELY:
        # Full circle (no directional info)
        center = sg.Point(lon, lat)
        # approximate circle in degrees (rough conversion)
        deg_per_m = 1.0 / 111_320.0
        circle = center.buffer(radius * deg_per_m)
        geojson = sg.mapping(circle)

    return DriftSector(
        station_id=meta.station_id,
        deploy_latitude=lat,
        deploy_longitude=lon,
        drift_bearing_deg=drift_bearing_deg,
        drift_distance_m=drift_distance_m,
        sector_half_angle_deg=sector_half_angle_deg,
        drift_scale_m=radius,
        sector_geojson=geojson,
        data_provenance=DataProvenance.CALCULATED,
    )


# ---------------------------------------------------------------------------
# Multi-station candidate
# ---------------------------------------------------------------------------

def _polygon_from_sector(sector: DriftSector) -> Optional[object]:
    if not HAS_SHAPELY or sector.sector_geojson is None:
        return None
    try:
        return sg.shape(sector.sector_geojson)
    except Exception:
        return None


def _centroid_of_polygon(poly: object) -> Tuple[float, float]:
    """Returns (lat, lon) of polygon centroid."""
    c = poly.centroid
    return float(c.y), float(c.x)  # shapely uses (x=lon, y=lat)


def compute_candidate_score(
    station_scores: Dict[str, float],
    overlap_fraction: float,
    data_quality: float,
) -> float:
    """
    Heuristic candidate_score.

    Formula (documented, NOT a probability formula):
        candidate_score = 0.50 * mean_station_score
                          + 0.30 * overlap_fraction
                          + 0.20 * data_quality

    Where:
        mean_station_score = average of per-station evidence_scores

    This is NOT a statistically calibrated probability.
    """
    if not station_scores:
        return 0.0
    mean_score = sum(station_scores.values()) / len(station_scores)
    overlap_fraction = max(0.0, min(1.0, overlap_fraction))
    data_quality = max(0.0, min(1.0, data_quality))
    raw = (0.50 * mean_score) + (0.30 * overlap_fraction) + (0.20 * data_quality)
    return max(0.0, min(1.0, round(raw, 4)))


def find_candidates(
    sectors: List[DriftSector],
    classifications: List[ClassificationResult],
) -> List[MultiStationCandidate]:
    """
    Find candidate zones where sectors from ≥2 stations overlap.

    Parameters
    ----------
    sectors         : drift sectors for each station
    classifications : classification results for each station

    Returns
    -------
    List of MultiStationCandidate (may be empty if no overlap)
    """
    if not HAS_SHAPELY:
        log.warning("Shapely not available — skipping multi-station intersection")
        return []

    class_map = {c.station_id: c for c in classifications}

    # Build polygon list
    polys = []
    for sector in sectors:
        poly = _polygon_from_sector(sector)
        if poly is not None and poly.is_valid and not poly.is_empty:
            polys.append((sector, poly))

    if len(polys) < 2:
        log.info("Fewer than 2 valid sector polygons — no multi-station candidates")
        return []

    candidates: List[MultiStationCandidate] = []
    candidate_counter = 0

    # Pairwise intersection (sufficient for ≤10 stations)
    for i in range(len(polys)):
        for j in range(i + 1, len(polys)):
            sec_i, poly_i = polys[i]
            sec_j, poly_j = polys[j]

            try:
                metric_i = so.transform(_TO_METRIC.transform, poly_i)
                metric_j = so.transform(_TO_METRIC.transform, poly_j)
                intersection = metric_i.intersection(metric_j)
            except Exception as exc:
                log.warning("Intersection error: %s", exc)
                continue

            if intersection.is_empty:
                continue

            overlap_area = intersection.area
            union_area   = metric_i.union(metric_j).area
            overlap_frac = overlap_area / max(union_area, 1e-12)

            intersection_wgs84 = so.transform(_TO_WGS84.transform, intersection)
            centroid_lat, centroid_lon = _centroid_of_polygon(intersection_wgs84)

            # Evidence scores
            c_i = class_map.get(sec_i.station_id)
            c_j = class_map.get(sec_j.station_id)
            station_scores = {}
            if c_i:
                station_scores[sec_i.station_id] = c_i.evidence_score
            if c_j:
                station_scores[sec_j.station_id] = c_j.evidence_score

            # Data quality: fraction of contributing stations with positive evidence
            quality = sum(1 for s in station_scores.values() if s > 0) / max(len(station_scores), 1)

            cand_score = compute_candidate_score(station_scores, overlap_frac, quality)

            # Classification: use the higher-priority label
            if c_i and c_j:
                # Prefer multi-sensor > metallic > thermal > insufficient
                priority = [
                    AnomalyType.POSSIBLE_HYDROTHERMAL_ASSOCIATED_ANOMALY,
                    AnomalyType.MULTISENSOR_ANOMALY,
                    AnomalyType.POSSIBLE_METALLIC_ANOMALY,
                    AnomalyType.POSSIBLE_THERMAL_ANOMALY,
                    AnomalyType.INSUFFICIENT_DATA,
                    AnomalyType.NONE,
                ]
                types_present = {c_i.anomaly_type, c_j.anomaly_type}
                chosen = AnomalyType.INSUFFICIENT_DATA
                for p in priority:
                    if p in types_present:
                        chosen = p
                        break
            else:
                chosen = AnomalyType.INSUFFICIENT_DATA

            candidate_counter += 1
            # Intersection was computed in the local metric CRS, so this is m².
            overlap_m2 = overlap_area

            candidates.append(MultiStationCandidate(
                candidate_id=f"CAND_{candidate_counter:03d}",
                station_ids=[sec_i.station_id, sec_j.station_id],
                centroid_lat=centroid_lat,
                centroid_lon=centroid_lon,
                overlap_area_m2=overlap_m2,
                overlap_geojson=sg.mapping(intersection_wgs84),
                station_scores=station_scores,
                candidate_score=cand_score,
                classification=chosen,
                data_provenance=DataProvenance.INFERRED,
            ))

    log.info("Found %d multi-station candidates", len(candidates))
    return candidates
