"""
Telemetry validator.

Checks:
  - Required columns present
  - Timestamp parseable
  - Physical value ranges (configurable)
  - Missing / null field counting

Does NOT modify the data — reports issues only.
"""

from __future__ import annotations

import logging
from typing import List, Optional

import pandas as pd

from backend.models.schemas import (
    DataProvenance,
    FieldIssue,
    ValidationReport,
    ValidationStatus,
)

log = logging.getLogger(__name__)

# ---------------------------------------------------------------------------
# Physical range limits
# These are conservative bounds; out-of-range rows get WARNING, not ERROR.
# ---------------------------------------------------------------------------

RANGE_CHECKS = {
    "water_temp_c":    (-5.0,  50.0),
    "imu_die_temp_c":  (-40.0, 85.0),
    "accel_x_g":       (-8.0,  8.0),
    "accel_y_g":       (-8.0,  8.0),
    "accel_z_g":       (-8.0,  8.0),
    "gyro_x_dps":      (-500.0, 500.0),
    "gyro_y_dps":      (-500.0, 500.0),
    "gyro_z_dps":      (-500.0, 500.0),
    "heading_deg":     (0.0,   360.0),
    "roll_deg":        (-180.0, 180.0),
    "pitch_deg":       (-90.0,  90.0),
    "pi_anomaly_strength": (0.0, 1e9),
    "latitude":        (-90.0,  90.0),
    "longitude":       (-180.0, 180.0),
}

# Columns that MUST be present (as a set; at least one timestamp required)
TIMESTAMP_COLS = {"timestamp_unix", "timestamp_rtc"}


def validate(
    df: pd.DataFrame,
    station_id: str,
    max_row_issues: int = 50,
) -> ValidationReport:
    """
    Validate a normalised telemetry DataFrame.

    Parameters
    ----------
    df           : normalised DataFrame from reader.read_telemetry()
    station_id   : identifier string for error messages
    max_row_issues : cap on per-row field issues logged (avoids noise)

    Returns
    -------
    ValidationReport with issues list and overall_status
    """
    issues: List[FieldIssue] = []
    total_rows = len(df)

    # ---- 1. Timestamp presence -------------------------------------------
    has_ts = any(c in df.columns for c in TIMESTAMP_COLS)
    if not has_ts:
        issues.append(FieldIssue(
            field="timestamp",
            message="No timestamp column found (expected timestamp_unix or timestamp_rtc)",
            status=ValidationStatus.ERROR,
        ))

    # ---- 2. Physical range checks ----------------------------------------
    row_issue_count = 0
    for col, (lo, hi) in RANGE_CHECKS.items():
        if col not in df.columns:
            continue
        series = pd.to_numeric(df[col], errors="coerce")
        bad = series.dropna()
        bad = bad[(bad < lo) | (bad > hi)]
        if not bad.empty and row_issue_count < max_row_issues:
            for idx in bad.index[:5]:  # show first 5 bad rows per column
                issues.append(FieldIssue(
                    field=col,
                    row=int(idx),
                    message=f"Value {bad[idx]:.4g} outside expected range [{lo}, {hi}]",
                    status=ValidationStatus.WARNING,
                ))
                row_issue_count += 1

    # ---- 3. Null / missing counts ----------------------------------------
    key_cols = [
        "water_temp_c", "mag_x_ut", "pi_anomaly_strength",
        "accel_x_g", "heading_deg",
    ]
    for col in key_cols:
        if col not in df.columns:
            issues.append(FieldIssue(
                field=col,
                message=f"Column '{col}' not present in telemetry",
                status=ValidationStatus.WARNING,
            ))
            continue
        null_count = df[col].isna().sum()
        if null_count > 0:
            pct = 100.0 * null_count / max(total_rows, 1)
            issues.append(FieldIssue(
                field=col,
                message=f"{null_count} null values ({pct:.1f}%)",
                status=ValidationStatus.WARNING if pct < 50 else ValidationStatus.ERROR,
            ))

    # ---- 4. Overall status ----------------------------------------------
    errors = [i for i in issues if i.status == ValidationStatus.ERROR]
    warns  = [i for i in issues if i.status == ValidationStatus.WARNING]
    if errors:
        overall = ValidationStatus.ERROR
    elif warns:
        overall = ValidationStatus.WARNING
    else:
        overall = ValidationStatus.OK

    # ---- 5. Valid row count (no ERROR-level validity flags) -------------
    # Use firmware validity flags if available
    valid_mask = pd.Series([True] * total_rows, index=df.index)
    for flag_col in ("imu_valid", "pt100_valid", "mag_valid", "pi_valid"):
        if flag_col in df.columns:
            flag_series = df[flag_col].fillna(True)
            try:
                flag_series = flag_series.astype(bool)
                valid_mask &= flag_series
            except Exception:
                pass
    valid_rows = int(valid_mask.sum())

    log.info(
        "Validation [%s]: %d/%d rows valid, %d errors, %d warnings",
        station_id, valid_rows, total_rows, len(errors), len(warns),
    )

    return ValidationReport(
        station_id=station_id,
        total_rows=total_rows,
        valid_rows=valid_rows,
        issues=issues,
        overall_status=overall,
        data_provenance=DataProvenance.MEASURED,
    )
