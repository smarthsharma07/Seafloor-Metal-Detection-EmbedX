# MoES Deployable Underwater Seafloor Metal-Detection Sensor Package

SIH 2026 prototype for a low-cost, deployable ocean-bottom sensor package and desktop analysis dashboard for identifying and localizing possible geophysical anomalies.

## Project Information

- **Project Title:** Low-Cost Deployable Seafloor Metal Detection Sensor for Ocean Resource Exploration
- **PS ID:** 26064
- **PS Title:** Ministry of Earth Sciences / NCPOR seafloor metal-detection and ocean resource exploration problem
- **Category:** Software and Hardware Prototype
- **Theme:** Ocean Technology, Seafloor Exploration, and Marine Instrumentation
- **Prototype Status:** Bench bring-up and simulation verified; hardware validation remains deployment-dependent

## Problem Statement

Ocean resource exploration requires a practical way to collect and inspect multi-sensor measurements from a deployable seafloor package. A prototype must account for sensor validity, pulse-induction and magnetic responses, temperature, motion, recovery logging, and deployment drift without presenting an uncertain location as an exact target or claiming that an anomaly is a confirmed mineral deposit.

## Proposed Solution

The system combines an ESP32-based underwater sensor package with a local FastAPI desktop dashboard. The package collects pulse-induction, magnetic, temperature, inertial, and time data, stores raw telemetry locally, and exposes recovery Wi-Fi/serial access. The dashboard accepts CSV/JSON telemetry, validates and preserves the raw input, calculates conservative anomaly evidence, estimates drift uncertainty, generates IDW interpolated intensity maps, and exports results for review.

This is a decision-support prototype. It does not confirm ore, confirm a mineral, or replace geological and geophysical validation.

## Key Features

- ESP32 firmware with modular sensor drivers for MPU6050, RM3100, MAX31865/PT100, and DS3231.
- Pulse-induction waveform sampling with baseline, decay, noise, and anomaly-strength features.
- Sensor validity flags and diagnostics for each acquisition path.
- DEMO_MODE synthetic telemetry for an end-to-end demonstration without physical hardware.
- Local LittleFS CSV logging and recovery Wi-Fi/serial endpoints.
- FastAPI dashboard for CSV/JSON upload, validation, processing, mapping, and export.
- Configurable **Evidence / Confidence Score** using a rule-based heuristic. **Rule-based, not statistically calibrated.**
- Conservative result labels: `POSSIBLE_METALLIC_ANOMALY`, `POSSIBLE_THERMAL_ANOMALY`, `POSSIBLE_HYDROTHERMAL_ASSOCIATED_ANOMALY`, `MULTISENSOR_ANOMALY`, and `INSUFFICIENT_DATA`.
- IDW interpolation for the prototype heatmap, labelled **Interpolated anomaly intensity** and never treated as a direct measurement.
- Distinct map layers for deployment points, measured anomaly points, interpolated heatmap, drift uncertainty sectors, and candidate overlap regions.
- Candidate scoring based on station evidence scores, spatial overlap, and data quality. No probability-combination formula is used.
- Explicit provenance metadata on results: `MEASURED`, `CALCULATED`, `INFERRED`, or `DEMO`.
- GeoJSON and raw/processed export bundles for reproducible review.

## Scientific and Reporting Guardrails

- The evidence score is a configurable heuristic, not a calibrated probability or statistical certainty.
- The 400 m value is a deployment drift scale / uncertainty extent, not an exact target position.
- Interpolated heatmap cells are calculated estimates between measured points, not direct measurements.
- Raw telemetry is retained separately and is never overwritten by processed data.
- The system reports possible anomalies only. It never outputs `confirmed ore` or `confirmed mineral`.
- Synthetic demonstration records are explicitly tagged `DEMO`.

## Technology Stack

### Embedded package

- PlatformIO
- C++ with the Arduino ESP32 framework
- ESP32 WROOM-32E / ESP32-compatible development board
- I2C, SPI, ADC, GPIO, LittleFS, serial CLI, and recovery Wi-Fi

### Desktop software

- Python 3.10+
- FastAPI and Uvicorn
- HTML, CSS, and vanilla JavaScript frontend
- NumPy, pandas, SciPy, Shapely, and PyProj
- GeoJSON export for GIS-compatible review

## Architecture

```text
                         Surface operator
                                |
                                v
                    FastAPI desktop dashboard
              upload -> validate -> process -> export
                                |
       +------------------------+------------------------+
       |                         |                        |
       v                         v                        v
  Raw telemetry          Heuristic evidence         Map/layer outputs
  preserved              and candidate score       IDW + drift sectors
       ^                         ^                        ^
       |                         |                        |
  ESP32 recovery Wi-Fi    Sensor validity         GeoJSON/report bundle
       ^
       |
  Seafloor sensor package
  PI coil | RM3100 | PT100 | MPU6050 | DS3231
```

Detailed firmware architecture is documented in [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md). The desktop implementation is organized under `desktop_software/backend/` by ingestion, preprocessing, feature extraction, detection, classification, localization, mapping, and reporting responsibilities.

## Repository Structure

```text
MoES_Sensor_Project_Proposal_V2/
├── README.md
├── platformio.ini
├── src/                              # ESP32 firmware
│   ├── main.cpp
│   ├── config.h
│   ├── sensors/                      # MPU6050, RM3100, PT100, DS3231
│   ├── pi/                           # Pulse-induction detector
│   ├── fusion/                       # Attitude, heading, drift fusion
│   ├── logging/                      # Local telemetry logging
│   ├── communication/                # Recovery Wi-Fi and HTTP endpoints
│   └── diagnostics/                  # Hardware self-tests
├── desktop_software/                 # FastAPI analysis dashboard
│   ├── main.py
│   ├── run_dashboard.bat
│   ├── backend/
│   │   ├── ingestion/ preprocessing/ features/
│   │   ├── detection/ classification/ localization/
│   │   ├── mapping/ reports/ models/ demo/
│   ├── frontend/                     # Dashboard HTML, CSS, JavaScript
│   └── data/                         # uploads, demo data, exports
├── docs/                             # Engineering and verification documents
├── test/                             # Firmware-oriented mathematical tests
├── assets/
│   ├── screenshots/                  # Dashboard and prototype images
│   ├── circuit_schematics/           # Wiring diagrams and circuit files
│   └── vessel_schematics_cad/        # Vessel/platform CAD and schematics
└── submission/                       # Presentation and demo links
```

### What goes where?

| Item | Location |
| --- | --- |
| ESP32 firmware | `src/` |
| Desktop dashboard | `desktop_software/` |
| Architecture, requirements, calibration, and test documents | `docs/` |
| Firmware mathematical tests | `test/` |
| Dashboard screenshots and prototype photographs | `assets/screenshots/` |
| Circuit schematics, wiring, and PCB references | `assets/circuit_schematics/` |
| Vessel schematics, mechanical drawings, and CAD files | `assets/vessel_schematics_cad/` |
| Final presentation | `submission/` |
| Demo video link | `submission/DEMO.md` |
| Project overview | `README.md` |

## Installation

Clone the repository and install the desktop dependencies:

```powershell
git clone <YOUR_REPOSITORY_URL>
cd MoES_Sensor_Project_Proposal_V2
python -m venv .venv
.\.venv\Scripts\Activate.ps1
python -m pip install -r desktop_software\requirements.txt
```

PlatformIO can build the firmware from the repository root after installing PlatformIO Core or the PlatformIO IDE extension.

## Run the Desktop Prototype

From the repository root on Windows:

```powershell
desktop_software\run_dashboard.bat
```

Then open [http://127.0.0.1:8000/](http://127.0.0.1:8000/) in a browser. The dashboard must be served through this URL; opening `dashboard.html` directly from the filesystem will not load the dashboard JavaScript correctly.

For a direct Python launch:

```powershell
cd desktop_software
$env:PYTHONPATH = (Get-Location).Path
python -m uvicorn main:app --host 127.0.0.1 --port 8000
```

Use **Run DEMO (synthetic)** for a complete local walkthrough, or upload compatible CSV/JSON telemetry through the dashboard. Synthetic records are marked `DEMO` in the resulting exports.

## Firmware Build and Test

```bash
pio run
python test/test_fusion_math.py
python test/test_prototype_corrections.py
```

The firmware must be bench-tested with the actual sensor wiring before field deployment. Hardware assumptions and unresolved items are recorded in [docs/ASSUMPTIONS.md](docs/ASSUMPTIONS.md) and [docs/KNOWN_LIMITATIONS.md](docs/KNOWN_LIMITATIONS.md).

## Documentation and Submission Files

- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md): firmware architecture and module map
- [docs/DATA_FORMAT.md](docs/DATA_FORMAT.md): telemetry and data contracts
- [docs/SENSOR_FUSION.md](docs/SENSOR_FUSION.md): heading, motion, and drift handling
- [docs/TEST_PLAN.md](docs/TEST_PLAN.md): verification plan
- [assets/screenshots/README.md](assets/screenshots/README.md): screenshot naming guidance
- [assets/circuit_schematics/README.md](assets/circuit_schematics/README.md): circuit asset guidance
- [assets/vessel_schematics_cad/README.md](assets/vessel_schematics_cad/README.md): vessel/CAD asset guidance
- [submission/PRESENTATION.md](submission/PRESENTATION.md): presentation placeholder
- [submission/DEMO.md](submission/DEMO.md): demo video/link placeholder
- [desktop_software/data/demo/synthetic_possible_anomaly_points_20.csv](desktop_software/data/demo/synthetic_possible_anomaly_points_20.csv): 20-row synthetic survey-track dataset

## Future Scope

- Validate the package with controlled tank, coastal, and vessel trials.
- Replace provisional sensor parameters with calibrated field measurements.
- Add authenticated and encrypted recovery communications for operational deployments.
- Add GIS project packaging and richer survey-line planning after the prototype workflow is stable.
- Evaluate advanced statistical or machine-learning methods only when a representative labelled dataset is available; they are outside the current prototype scope.

## Security and Data Handling

Do not commit passwords, API keys, access tokens, private telemetry, `.env` files containing secrets, or sensitive vessel/security information. Keep raw data in its own input area and use the dashboard export bundle for derived results. Review CAD and schematic files for restricted information before publishing the repository.

## Team Additions Before Submission

Add the final team, institute, mentor, repository URL, presentation, demo link, screenshots, circuit schematics, vessel CAD, and field-validation evidence to the marked folders before submitting the GitHub link.

## Sample Dataset

The repository includes a 20-row synthetic survey-track CSV at [desktop_software/data/demo/synthetic_possible_anomaly_points_20.csv](desktop_software/data/demo/synthetic_possible_anomaly_points_20.csv). It contains DEMO-only measurements with a central elevated magnetic/PI/temperature response and surrounding lower-signal points.

To test it, start the dashboard, enter a station ID such as `DEMO_ORE_TRACK`, set the deployment location near `14.98000, 74.01000`, keep the depth at `250 m` and drift scale at `400 m`, select the CSV, and choose **Upload & Process**. The file is synthetic and represents possible anomaly evidence for software testing; it does not represent confirmed ore or a real field measurement.
