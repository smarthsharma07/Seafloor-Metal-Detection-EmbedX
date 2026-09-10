/**
 * MoES Dashboard — Frontend JavaScript
 * SIH 2026 Prototype
 *
 * Responsibilities:
 *   - Server health polling
 *   - Upload / DEMO pipeline trigger
 *   - Leaflet map with 5 distinct layer groups
 *   - IDW heatmap rendering (canvas overlay)
 *   - Plotly sensor time-series chart
 *   - Classification card
 *   - Threshold config form
 *   - GeoJSON / JSON export
 */

"use strict";

// ── Map setup ─────────────────────────────────────────────────────────────

const map = L.map("map", { zoomControl: true }).setView([14.98, 74.01], 13);

L.tileLayer(
  "https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png",
  { attribution: "© OpenStreetMap contributors", maxZoom: 19 }
).addTo(map);

// Five visually distinct layer groups
const layers = {
    deploy:     L.layerGroup().addTo(map),
    anomaly:    L.layerGroup().addTo(map),
    sectors:    L.layerGroup().addTo(map),
    heatmap:    L.layerGroup().addTo(map),
  candidates: L.layerGroup().addTo(map),
};

// Layer toggle checkboxes
document.getElementById("tog-deploy").addEventListener("change",     e => toggleLayer("deploy",     e.target.checked));
document.getElementById("tog-anomaly").addEventListener("change",    e => toggleLayer("anomaly",    e.target.checked));
document.getElementById("tog-sectors").addEventListener("change",    e => toggleLayer("sectors",    e.target.checked));
document.getElementById("tog-heatmap").addEventListener("change",    e => toggleLayer("heatmap",    e.target.checked));
document.getElementById("tog-candidates").addEventListener("change", e => toggleLayer("candidates", e.target.checked));

function toggleLayer(name, visible) {
  if (visible) map.addLayer(layers[name]);
  else         map.removeLayer(layers[name]);
}

// ── State ─────────────────────────────────────────────────────────────────

let state = {
  geojson:     null,
  rawBundle:   null,
  stations:    [],
  selectedStation: null,
};

// ── Utilities ─────────────────────────────────────────────────────────────

async function api(method, path, body = null, isForm = false) {
  const opts = { method };
  if (body && isForm) {
    opts.body = body;
  } else if (body) {
    opts.headers = { "Content-Type": "application/json" };
    opts.body = JSON.stringify(body);
  }
  const resp = await fetch(path, opts);
  if (!resp.ok) {
    const err = await resp.json().catch(() => ({ detail: resp.statusText }));
    throw new Error(err.detail || resp.statusText);
  }
  return resp.json();
}

function scoreColor(score) {
  if (score >= 0.6) return "#ef4444";
  if (score >= 0.35) return "#f59e0b";
  return "#22c55e";
}

function anomalyColor(type) {
  const map = {
    "POSSIBLE_METALLIC_ANOMALY":               "#dc2626",
    "POSSIBLE_THERMAL_ANOMALY":                "#f97316",
    "POSSIBLE_HYDROTHERMAL_ASSOCIATED_ANOMALY":"#a855f7",
    "MULTISENSOR_ANOMALY":                     "#eab308",
    "INSUFFICIENT_DATA":                       "#6b7280",
    "NONE":                                    "#22c55e",
  };
  return map[type] || "#6b7280";
}

// ── Status polling ────────────────────────────────────────────────────────

async function pollStatus() {
  try {
    const s = await api("GET", "/api/status");
    document.getElementById("status-dot").className = "dot dot-green";
    document.getElementById("status-label").textContent = "Connected";
    if (s.has_results && !state.geojson) await refreshResults();
  } catch {
    document.getElementById("status-dot").className = "dot dot-red";
    document.getElementById("status-label").textContent = "Server offline";
  }
}

setInterval(pollStatus, 10_000);
pollStatus();

// ── Upload ────────────────────────────────────────────────────────────────

document.getElementById("btn-upload").addEventListener("click", async () => {
  const file = document.getElementById("inp-file").files[0];
  if (!file) { alert("Please select a telemetry file."); return; }

  const fd = new FormData();
  fd.append("file",          file);
  fd.append("station_id",    document.getElementById("inp-station-id").value || "STATION_A");
  fd.append("latitude",      document.getElementById("inp-lat").value);
  fd.append("longitude",     document.getElementById("inp-lon").value);
  fd.append("depth_m",       document.getElementById("inp-depth").value || "");
  fd.append("drift_scale_m", document.getElementById("inp-drift").value || "400");
  fd.append("notes",         "");

  setLoading(true);
  try {
    await api("POST", "/api/upload", fd, true);
    await refreshResults();
  } catch (err) {
    alert("Upload error: " + err.message);
  } finally {
    setLoading(false);
  }
});

// ── DEMO ──────────────────────────────────────────────────────────────────

document.getElementById("btn-demo").addEventListener("click", async () => {
  setLoading(true);
  try {
    await api("POST", "/api/run_demo");
    await refreshResults();
  } catch (err) {
    alert("Demo error: " + err.message);
  } finally {
    setLoading(false);
  }
});

function setLoading(on) {
  document.getElementById("btn-upload").disabled = on;
  document.getElementById("btn-demo").disabled   = on;
  if (on) {
    document.getElementById("status-label").textContent = "Processing…";
    document.getElementById("status-dot").className = "dot dot-gray";
  }
}

// ── Refresh results ───────────────────────────────────────────────────────

async function refreshResults() {
  const [geojson, bundle] = await Promise.all([
    api("GET", "/api/results"),
    api("GET", "/api/results/raw"),
  ]);
  state.geojson   = geojson;
  state.rawBundle = bundle;
  state.stations  = bundle.stations || [];

  renderMap(geojson);
  renderStationList(bundle);
  renderCandidateList(bundle.candidates || []);
  populateStationSelects(bundle.stations || []);
  await loadThresholds();

  document.getElementById("status-dot").className = "dot dot-green";
  document.getElementById("status-label").textContent = "Results loaded";
}

// ── Map rendering ─────────────────────────────────────────────────────────

function renderMap(geojson) {
  // Clear all layers
  Object.values(layers).forEach(lg => lg.clearLayers());

  const bounds = [];

  geojson.features.forEach(feat => {
    const p = feat.properties;
    const geom = feat.geometry;

    switch (p.layer) {

      case "deployment_points": {
        const [lon, lat] = geom.coordinates;
        bounds.push([lat, lon]);
        const m = L.circleMarker([lat, lon], {
          radius: 9, color: "#1d4ed8", fillColor: "#2563eb",
          fillOpacity: 0.85, weight: 2,
        }).bindPopup(popupDeploy(p));
        layers.deploy.addLayer(m);
        break;
      }

      case "measured_anomaly_points": {
        const [lon, lat] = geom.coordinates;
        const color = anomalyColor(p.anomaly_type);
        const m = L.circleMarker([lat, lon], {
          radius: 10, color, fillColor: color,
          fillOpacity: 0.8, weight: 2.5,
        }).bindPopup(popupAnomaly(p));
        layers.anomaly.addLayer(m);
        break;
      }

      case "drift_sectors": {
        if (geom.type === "Polygon" || geom.type === "MultiPolygon") {
          const latlngs = geojsonCoordsToLatLng(geom);
          const poly = L.polygon(latlngs, {
            color: "#ca8a04", fillColor: "#fde047",
            fillOpacity: 0.18, weight: 1.8, dashArray: "5,4",
          }).bindPopup(popupSector(p));
          layers.sectors.addLayer(poly);
        }
        break;
      }

      case "interpolated_heatmap": {
        // Render as tiny coloured rectangles (lightweight substitute for canvas heatmap)
        const [lon, lat] = geom.coordinates;
        const v = p.value;
        if (v < 0.05) break;
        const col = heatColor(v);
        const cell = L.circleMarker([lat, lon], {
          radius: 6, fillColor: col, fillOpacity: 0.42,
          color: "transparent", weight: 0,
        }).bindPopup(`<b>Interpolated anomaly intensity</b><br>Value: ${v.toFixed(3)}<br><em>Not a direct measurement</em><br>Provenance: <code>${p.data_provenance}</code>`);
        layers.heatmap.addLayer(cell);
        break;
      }

      case "candidate_overlap_regions": {
        const color = "#7c3aed";
        if (geom.type === "Polygon" || geom.type === "MultiPolygon") {
          const latlngs = geojsonCoordsToLatLng(geom);
          const poly = L.polygon(latlngs, {
            color,
            fillColor: color,
            fillOpacity: 0.22,
            weight: 2.2,
          }).bindPopup(popupCandidate(p));
          layers.candidates.addLayer(poly);
        } else {
          const [lon, lat] = geom.coordinates;
          const m = L.circleMarker([lat, lon], {
            radius: 13, color, fillColor: color,
            fillOpacity: 0.55, weight: 2.5,
          }).bindPopup(popupCandidate(p));
          layers.candidates.addLayer(m);
          bounds.push([lat, lon]);
        }
        break;
      }
    }
  });

  if (bounds.length) map.fitBounds(bounds, { padding: [40, 40] });
}

function heatColor(v) {
  // 0→blue, 0.5→yellow, 1→red
  const r = Math.round(Math.min(1, v * 2) * 255);
  const b = Math.round(Math.min(1, (1 - v) * 2) * 255);
  return `rgb(${r},80,${b})`;
}

function geojsonCoordsToLatLng(geom) {
  if (geom.type === "Polygon") {
    return geom.coordinates.map(ring => ring.map(([lon, lat]) => [lat, lon]));
  }
  return geom.coordinates.map(poly => poly.map(ring => ring.map(([lon, lat]) => [lat, lon])));
}

// ── Popup builders ────────────────────────────────────────────────────────

function popupDeploy(p) {
  return `<b>Deployment location: ${p.station_id}</b><br>
  Drift scale / uncertainty extent: ${p.drift_radius_m ?? p.drift_scale_m} m<br>
  Depth: ${p.depth_m ?? "—"} m<br>
  Provenance: <code>${p.data_provenance}</code>`;
}

function popupAnomaly(p) {
  const pct = (p.evidence_score * 100).toFixed(1);
  return `<b>Measured anomaly point — ${p.station_id}</b><br>
  Type: <b>${p.anomaly_type}</b><br>
  Evidence / Confidence Score: <b>${pct}%</b><br>
  <em>Rule-based, not statistically calibrated</em><br>
  Timestamp: ${p.representative_timestamp || "—"}<br>
  Window: ${p.start_timestamp || "—"} to ${p.end_timestamp || "—"}<br>
  Location: ${p.latitude != null ? `${Number(p.latitude).toFixed(6)}, ${Number(p.longitude).toFixed(6)}` : "—"}<br>
  PI: ${p.pi_strength ?? "—"} | Temperature: ${p.temperature ?? "—"} °C | ΔT: ${p.temperature_delta ?? "—"} °C<br>
  Magnetic: ${p.magnetic_strength ?? "—"} | Heading: ${p.heading ?? "—"}<br>
  Evidence flags: ${(p.evidence_flags || p.contributing_channels || []).join(", ") || "—"}<br>
  Provenance: <code>${p.data_provenance}</code>`;
}

function popupSector(p) {
  return `<b>Drift uncertainty sector — ${p.station_id}</b><br>
  Bearing: ${p.bearing_deg != null ? p.bearing_deg.toFixed(1)+"°" : "—"}<br>
  Radius: ${p.drift_radius_m} m<br>
  <em>${p.note}</em><br>
  Provenance: <code>${p.data_provenance}</code>`;
}

function popupCandidate(p) {
  const pct = (p.candidate_score * 100).toFixed(1);
  return `<b>Candidate overlap region: ${p.candidate_id}</b><br>
  Classification: <b>${p.classification}</b><br>
  Candidate Score: <b>${pct}%</b><br>
  <em>Heuristic score — not a probability</em><br>
  Stations: ${p.station_ids.join(", ")}<br>
  Overlap area: ${p.overlap_area_m2 != null ? (p.overlap_area_m2/1e6).toFixed(4)+" km²" : "—"}<br>
  Provenance: <code>${p.data_provenance}</code>`;
}

// ── Station list ──────────────────────────────────────────────────────────

function renderStationList(bundle) {
  const el = document.getElementById("station-list");
  const clsMap = {};
  (bundle.classifications || []).forEach(c => { clsMap[c.station_id] = c; });

  el.innerHTML = (bundle.stations || []).map(st => {
    const c = clsMap[st.station_id];
    const score = c ? (c.evidence_score * 100).toFixed(1) + "%" : "—";
    const type  = c ? c.anomaly_type : "—";
    return `<div class="station-item">
      <strong>${st.station_id}</strong>
      ${st.demo_mode ? '<span class="badge-demo">[DEMO]</span>' : ""}
      <div>Type: <b>${type}</b></div>
      <div>Evidence / Confidence Score: <b>${score}</b></div>
      <div style="color:#94a3b8;font-size:10px">${st.deploy_latitude.toFixed(5)}, ${st.deploy_longitude.toFixed(5)}</div>
    </div>`;
  }).join("") || '<p class="muted">No stations.</p>';
}

// ── Candidate list ────────────────────────────────────────────────────────

function renderCandidateList(candidates) {
  const el = document.getElementById("candidate-list");
  el.innerHTML = candidates.map(c => {
    const pct = (c.candidate_score * 100).toFixed(1);
    return `<div class="candidate-item">
      <strong>${c.candidate_id}</strong>
      <div>${c.classification}</div>
      <div>Candidate score: <b>${pct}%</b> <span style="font-size:10px;color:#94a3b8">(heuristic)</span></div>
      <div style="font-size:10px;color:#94a3b8">${c.station_ids.join(" + ")}</div>
    </div>`;
  }).join("") || '<p class="muted">No candidates found.</p>';
}

// ── Station selects ───────────────────────────────────────────────────────

function populateStationSelects(stations) {
  const sel  = document.getElementById("sel-station");
  const prev = sel.value;
  sel.innerHTML = '<option value="">— select station —</option>' +
    stations.map(s => `<option value="${s.station_id}">${s.station_id}</option>`).join("");
  if (prev) sel.value = prev;
}

document.getElementById("sel-station").addEventListener("change", async e => {
  const sid = e.target.value;
  if (!sid) return;
  state.selectedStation = sid;
  showClassificationCard(sid);
  await loadTimeSeries(sid);
});

document.getElementById("sel-channel").addEventListener("change", async () => {
  if (state.selectedStation) await loadTimeSeries(state.selectedStation);
});

// ── Classification card ───────────────────────────────────────────────────

function showClassificationCard(sid) {
  if (!state.rawBundle) return;
  const cls = (state.rawBundle.classifications || []).find(c => c.station_id === sid);
  const card = document.getElementById("classification-card");
  if (!cls) { card.classList.add("hidden"); return; }

  card.classList.remove("hidden");
  document.getElementById("class-type").textContent    = cls.anomaly_type;
  document.getElementById("class-type").style.color    = anomalyColor(cls.anomaly_type);
  document.getElementById("class-score-label").textContent = "Evidence / Confidence Score";
  document.getElementById("class-score").textContent   = (cls.evidence_score * 100).toFixed(1) + "%";
  document.getElementById("class-score").style.color   = scoreColor(cls.evidence_score);
  document.getElementById("class-channels").textContent = "Sensors: " + (cls.contributing_channels.join(", ") || "—");
  document.getElementById("class-rationale").textContent = "Rationale: " + cls.rationale;
  document.getElementById("class-provenance").textContent = "Provenance: " + cls.data_provenance;
}

// ── Time-series chart ─────────────────────────────────────────────────────

async function loadTimeSeries(sid) {
  const channel = document.getElementById("sel-channel").value;
  try {
    const data = await api("GET", `/api/telemetry/${sid}?max_rows=400`);
    const records = data.records || [];
    const xs = records.map(r => r.timestamp_rtc || r.timestamp_unix || "");
    const ys = records.map(r => r[channel] ?? null);

    Plotly.react("chart-ts", [{
      x: xs, y: ys,
      type: "scatter", mode: "lines",
      line: { color: "#3b82f6", width: 1.5 },
      name: channel,
    }], {
      paper_bgcolor: "#1e293b",
      plot_bgcolor:  "#0f172a",
      font:  { color: "#e2e8f0", size: 11 },
      xaxis: { gridcolor: "#334155", title: "" },
      yaxis: { gridcolor: "#334155", title: channel },
      margin: { l: 50, r: 10, t: 10, b: 40 },
    }, { displayModeBar: false, responsive: true });
  } catch (err) {
    console.warn("Time-series load error:", err);
  }
}

// ── Threshold config ──────────────────────────────────────────────────────

async function loadThresholds() {
  try {
    const cfg = await api("GET", "/api/config");
    const form = document.getElementById("threshold-form");
    form.innerHTML = Object.entries(cfg.thresholds).map(([k, v]) =>
      `<label>${k}</label><input type="number" step="any" name="${k}" value="${v}" style="width:100%;"/>`
    ).join("");
  } catch {}
}

document.getElementById("btn-save-config").addEventListener("click", async () => {
  const form = document.getElementById("threshold-form");
  const body = {};
  form.querySelectorAll("input").forEach(inp => {
    body[inp.name] = parseFloat(inp.value);
  });
  try {
    await api("POST", "/api/config", body);
    alert("Thresholds saved. Re-run pipeline to apply.");
  } catch (err) {
    alert("Error: " + err.message);
  }
});

// ── Export ────────────────────────────────────────────────────────────────

document.getElementById("btn-export-geojson").addEventListener("click", () => {
  if (!state.geojson) { alert("No results yet."); return; }
  downloadJSON(state.geojson, "moes_results.geojson");
});

document.getElementById("btn-export-json").addEventListener("click", () => {
  if (!state.rawBundle) { alert("No results yet."); return; }
  downloadJSON(state.rawBundle, "moes_export_bundle.json");
});

function downloadJSON(obj, filename) {
  const blob = new Blob([JSON.stringify(obj, null, 2)], { type: "application/json" });
  const url  = URL.createObjectURL(blob);
  const a    = document.createElement("a");
  a.href = url; a.download = filename; a.click();
  URL.revokeObjectURL(url);
}
