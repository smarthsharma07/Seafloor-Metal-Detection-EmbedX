"""
Telemetry reader: parses ESP32 CSV or JSON telemetry files.

Column aliases normalise the many possible header formats the firmware
may emit (snake_case, camelCase, abbreviated, legacy) into the canonical
field names used by TelemetryRecord.

Raw data is NEVER overwritten — the original file is left untouched.
"""

from __future__ import annotations

import json
import logging
from pathlib import Path
from typing import Any, Dict, List

import pandas as pd

log = logging.getLogger(__name__)

# ---------------------------------------------------------------------------
# Column alias table
# Canonical name -> list of alternative column names the firmware may emit
# ---------------------------------------------------------------------------

COLUMN_ALIASES: Dict[str, List[str]] = {
    "timestamp_unix":    ["ts_unix", "unix_ts", "epoch", "time_unix"],
    "timestamp_rtc":     ["ts_rtc", "rtc", "datetime", "timestamp"],
    "imu_valid":         ["imu_ok", "imu_valid"],
    "pt100_valid":       ["pt100_ok", "water_temp_valid", "temp_valid"],
    "mag_valid":         ["mag_ok", "mag_valid", "rm3100_valid"],
    "pi_valid":          ["pi_ok", "pi_valid"],
    "rtc_valid":         ["rtc_ok", "rtc_valid"],
    "gps_valid":         ["gps_ok", "gps_valid"],
    "accel_x_g":         ["ax", "accel_x", "acc_x_g"],
    "accel_y_g":         ["ay", "accel_y", "acc_y_g"],
    "accel_z_g":         ["az", "accel_z", "acc_z_g"],
    "gyro_x_dps":        ["gx", "gyro_x", "gyr_x_dps"],
    "gyro_y_dps":        ["gy", "gyro_y", "gyr_y_dps"],
    "gyro_z_dps":        ["gz", "gyro_z", "gyr_z_dps"],
    "imu_die_temp_c":    ["imu_temp", "imu_temperature", "mpu_die_temp", "imu_die_temperature_c"],
    "water_temp_c":      ["pt100_temp", "water_temperature_c", "water_temperature", "temp_c"],
    "mag_x_ut":          ["mx", "mag_x", "rm3100_x"],
    "mag_y_ut":          ["my", "mag_y", "rm3100_y"],
    "mag_z_ut":          ["mz", "mag_z", "rm3100_z"],
    "pi_baseline":       ["pi_base", "baseline"],
    "pi_peak":           ["peak"],
    "pi_early_decay":    ["early_decay", "pi_early"],
    "pi_late_decay":     ["late_decay", "pi_late"],
    "pi_anomaly_strength": ["pi_strength", "anomaly_strength"],
    "roll_deg":          ["roll"],
    "pitch_deg":         ["pitch"],
    "heading_deg":       ["heading", "yaw_deg", "yaw"],
    "drift_north_m":     ["dn", "drift_n"],
    "drift_east_m":      ["de", "drift_e"],
    "deployment_drift_radius_m": ["drift_radius", "uncertainty_radius", "drift_radius_m"],
    "latitude":          ["lat", "gps_lat"],
    "longitude":         ["lon", "lng", "gps_lon"],
}

# Build reverse alias map: alias -> canonical
_ALIAS_TO_CANONICAL: Dict[str, str] = {}
for _canon, _aliases in COLUMN_ALIASES.items():
    for _a in _aliases:
        _ALIAS_TO_CANONICAL[_a.lower()] = _canon
    _ALIAS_TO_CANONICAL[_canon.lower()] = _canon


def _normalise_columns(df: pd.DataFrame) -> pd.DataFrame:
    """Rename dataframe columns to canonical names using alias table."""
    rename_map = {}
    for col in df.columns:
        canon = _ALIAS_TO_CANONICAL.get(col.lower())
        if canon and col != canon:
            rename_map[col] = canon
    return df.rename(columns=rename_map)


def read_csv(path: Path | str) -> pd.DataFrame:
    """
    Read a telemetry CSV file.
    Returns a DataFrame with columns normalised to canonical names.
    """
    path = Path(path)
    log.info("Reading CSV: %s", path)
    df = pd.read_csv(path, low_memory=False)
    df = _normalise_columns(df)
    log.info("  %d rows, %d columns after normalisation", len(df), len(df.columns))
    return df


def read_json(path: Path | str) -> pd.DataFrame:
    """
    Read a telemetry JSON file.
    Accepts either a JSON array of objects, or NDJSON (one object per line).
    """
    path = Path(path)
    log.info("Reading JSON: %s", path)
    text = path.read_text(encoding="utf-8")

    records: List[Dict[str, Any]]
    try:
        payload = json.loads(text)
        if isinstance(payload, list):
            records = payload
        elif isinstance(payload, dict):
            # Maybe it's a wrapper like {"data": [...]}
            for key in ("data", "records", "samples", "telemetry"):
                if key in payload and isinstance(payload[key], list):
                    records = payload[key]
                    break
            else:
                records = [payload]
        else:
            raise ValueError("Unexpected JSON structure")
    except json.JSONDecodeError:
        # Try NDJSON
        records = []
        for line in text.splitlines():
            line = line.strip()
            if line:
                records.append(json.loads(line))

    df = pd.DataFrame(records)
    df = _normalise_columns(df)
    log.info("  %d rows, %d columns after normalisation", len(df), len(df.columns))
    return df


def read_telemetry(path: Path | str) -> pd.DataFrame:
    """Auto-detect CSV vs JSON and load the file."""
    path = Path(path)
    suffix = path.suffix.lower()
    if suffix in (".json", ".ndjson"):
        return read_json(path)
    elif suffix in (".csv", ".txt"):
        return read_csv(path)
    else:
        # Try CSV first, fall back to JSON
        try:
            return read_csv(path)
        except Exception:
            return read_json(path)
