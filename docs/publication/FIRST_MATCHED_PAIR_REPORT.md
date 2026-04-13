# First Matched Pair Report

Date: 2026-04-13

This report closes the remaining sim-side infrastructure gap identified in the native bring-up pass:

- real `ns-3` export verification
- one rendered GCS desktop/OpenGL smoke run
- one minimal matched pair
- publication pipeline verification on that pair

The matched pair in this report is not a real field pair. The companion side is a controlled bridge proxy using the real `GCSNetworkPlanner` and `NetworkPlannerRpi` binaries.

## 1. `ns-3` Export Verification

### Command used

```bash
env \
  NP_SCENARIO_ID=sim-lte-3-static-none-exportverify \
  NP_RUN_ID=20260413T130500Z \
  NP_LOG_ROOT="$PWD/logs" \
  UAVS=3 \
  BASE_STATIONS=2 \
  SIM_TIME=5 \
  SECURITY=none \
  LIVE=0 \
  ./sim/ns3/scripts/run_uav_lte.sh
```

### Real artifacts produced

Raw run directory:

- `logs/raw/sim-lte-3-static-none-exportverify/20260413T130500Z`

Produced by the actual `ns-3` workflow:

- `logs/raw/sim-lte-3-static-none-exportverify/20260413T130500Z/ns3_lte_flow_monitor.csv`
- `logs/raw/sim-lte-3-static-none-exportverify/20260413T130500Z/ns3_lte_metadata.json`

### Verification result

The export path is working under the publication schema.

Observed CSV properties:

- filename and location match the documented publication layout
- rows are emitted with:
  - `source=ns3_export`
  - `evidence_layer=simulator_export`
  - `metric_origin=flow_monitor`
- rows include scenario metadata columns expected by the analysis layer:
  - `scenario_id`
  - `run_id`
  - `rat`
  - `security_profile`
  - `uavs`
  - `base_stations`
  - `sim_time_s`

Observed metadata JSON properties:

- `schema_name = networkplanner_sim_export`
- `scenario_id = sim-lte-3-static-none-exportverify`
- `run_id = 20260413T130500Z`
- `rat = lte`
- `security_profile = none`
- `uav_count = 3`
- `base_station_count = 2`
- `sim_time_s = 5`
- `flow_monitor_evidence_layer = simulator_export`
- `live_snapshot_evidence_layer = ui_visualization_only`

### Example export row

```csv
sim-lte-3-static-none-exportverify,20260413T130500Z,ns3_export,simulator_export,flow_monitor,lte,none,3,2,5,control_downlink,1,10.0.0.1,7.0.0.2,344,0.007126834666666667,21.618375,0.0125,0.0,,,6,2,512,0,0.0,0.0
```

## 2. Rendered GCS Verification

### Launch mode

This run was executed in the real desktop/OpenGL path, not in `offscreen` software rendering.

Environment checks before launch:

- `DISPLAY=:0`
- `WAYLAND_DISPLAY=wayland-0`
- `glxinfo -B` reported direct rendering on Intel Mesa OpenGL 4.6

### Command used

```bash
env \
  NP_SCENARIO_ID=sim-lte-3-static-none-gcsrender \
  NP_RUN_ID=20260413T131500Z \
  NP_LOG_ROOT=/tmp/gcs-render-verify \
  NP_RAT=lte \
  NP_SECURITY_PROFILE=none \
  NS3_SIM_PORT=45486 \
  NPGCS_PILOT_BIND_HOST=127.0.0.1 \
  NPGCS_PILOT_BIND_PORT=24651 \
  NPGCS_PILOT_REMOTE_HOST=127.0.0.1 \
  NPGCS_PILOT_REMOTE_PORT=24652 \
  NPGCS_MODEM_BIND_HOST=127.0.0.1 \
  NPGCS_MODEM_BIND_PORT=24681 \
  NPGCS_MODEM_REMOTE_HOST=127.0.0.1 \
  NPGCS_MODEM_REMOTE_PORT=24682 \
  NPGCS_GIMBAL_BIND_HOST=127.0.0.1 \
  NPGCS_GIMBAL_BIND_PORT=24691 \
  NPGCS_GIMBAL_REMOTE_HOST=127.0.0.1 \
  NPGCS_GIMBAL_REMOTE_PORT=24692 \
  DISPLAY=:0 \
  WAYLAND_DISPLAY=wayland-0 \
  QSG_INFO=1 \
  ./build/host-verify/bin/NetworkPlannerGCS
```

### Runtime observation

Observed rendered-path evidence from the real process:

- `threaded render loop`
- `Creating QRhi with backend OpenGL`
- `Created OpenGL context`
- `OpenGL VENDOR: Intel`
- `OpenGL RENDERER: Mesa Intel(R) Iris(R) Xe Graphics (RPL-U)`
- `OpenGL VERSION: 4.6 ...`

Observed window presence:

- `xwininfo` showed a real `NetworkPlannerGCS` top-level window

One live snapshot was injected to the real `Ns3SimulationFeed` UDP port. The GCS produced publication logs in the rendered path.

### Rendered-path artifacts produced

- `/tmp/gcs-render-verify/raw/sim-lte-3-static-none-gcsrender/20260413T131500Z/gcs_events.jsonl`
- `/tmp/gcs-render-verify/raw/sim-lte-3-static-none-gcsrender/20260413T131500Z/gcs_metadata.json`

Observed rendered-path event counts:

- `sync_status: 1`
- `sim_snapshot: 1`
- `selection_context: 1`

### Caveat

The rendered path still reported map style authentication failure:

- `loading style failed: HTTP status code 401`

This is a map/style credential issue, not a publication logger failure. The rendered OpenGL startup and publication logging path both worked despite the map style error.

## 3. Matched Pair Inventory

### Pair definition

This pass created the smallest convincing pair that exercises:

- real simulator export artifacts
- real GCS publication logs
- real RPi publication logs
- normalization
- summarization
- comparison

The pair shares:

- `run_id = 20260413T133000Z`
- `rat = lte`
- `security_profile = none`
- `uav_count = 3`
- `motion = static`

The domains differ intentionally:

- sim run: real `ns-3` export
- companion run: controlled bridge proxy using real GCS/RPi binaries

### Run A: simulation-side export

- `scenario_id = sim-lte-3-static-none-firstpair`
- `run_id = 20260413T133000Z`
- role: simulator export

Artifacts present:

- `logs/raw/sim-lte-3-static-none-firstpair/20260413T133000Z/ns3_lte_flow_monitor.csv`
- `logs/raw/sim-lte-3-static-none-firstpair/20260413T133000Z/ns3_lte_metadata.json`

GCS logs exist:

- no

RPi logs exist:

- no

`ns-3` exports exist:

- yes

### Run B: controlled bridge companion

- `scenario_id = field-lte-3-static-none-bridgeproxy`
- `run_id = 20260413T133000Z`
- role: controlled bridge proxy, not real field hardware

Launch components:

- rendered `NetworkPlannerGCS`
- native `NetworkPlannerRpi --simulation --ns3-live`
- one shared synthetic live snapshot injected into both real binaries

Artifacts present:

- `logs/raw/field-lte-3-static-none-bridgeproxy/20260413T133000Z/gcs_events.jsonl`
- `logs/raw/field-lte-3-static-none-bridgeproxy/20260413T133000Z/gcs_metadata.json`
- `logs/raw/field-lte-3-static-none-bridgeproxy/20260413T133000Z/rpi_bridge_events.jsonl`
- `logs/raw/field-lte-3-static-none-bridgeproxy/20260413T133000Z/rpi_bridge_metadata.json`

GCS logs exist:

- yes

RPi logs exist:

- yes

`ns-3` exports exist:

- no

### Companion run observations

Observed GCS event counts:

- `sync_status: 1`
- `sim_snapshot: 1`
- `selection_context: 1`
- `telemetry_rx: 9`
- `battery_sample: 3`

Observed RPi event counts:

- `sync_status: 1`
- `sim_snapshot: 1`
- `packet_forward: 13`

Example GCS companion rows:

```json
{"event_id":"gcs-000005","event_type":"battery_sample","evidence_layer":"field_ground_truth","message_name":"SYS_STATUS","metric_origin":"direct_observation","packet_loss_pct":1,"rat":"lte","run_id":"20260413T133000Z","scenario_id":"field-lte-3-static-none-bridgeproxy","schema_version":1,"security_profile":"none","sequence_id":1,"source":"gcs","status":"received","timestamp_monotonic_ms":20563,"timestamp_utc":"2026-04-13T05:10:50.273Z","uav_id":1,"battery_percentage":92,"battery_voltage":12.1,"battery_current_milliamps":1500}
{"event_id":"gcs-000009","event_type":"battery_sample","evidence_layer":"field_ground_truth","message_name":"SYS_STATUS","metric_origin":"direct_observation","packet_loss_pct":1.3,"rat":"lte","run_id":"20260413T133000Z","scenario_id":"field-lte-3-static-none-bridgeproxy","schema_version":1,"security_profile":"none","sequence_id":5,"source":"gcs","status":"received","timestamp_monotonic_ms":20580,"timestamp_utc":"2026-04-13T05:10:50.290Z","uav_id":2,"battery_percentage":87,"battery_voltage":11.9,"battery_current_milliamps":1680}
```

Example RPi companion row:

```json
{"bytes":468,"channel":"pilot","event_id":"rpi_bridge-000015","event_type":"packet_forward","evidence_layer":"field_ground_truth","metric_origin":"direct_observation","rat":"lte","remote_host":"127.0.0.1","remote_port":24751,"run_id":"20260413T133000Z","scenario_id":"field-lte-3-static-none-bridgeproxy","schema_version":1,"security_profile":"none","source":"rpi_bridge","status":"tx","timestamp_monotonic_ms":12094,"timestamp_utc":"2026-04-13T05:10:50.232Z"}
```

## 4. Pipeline Outputs

### Normalize

Commands:

```bash
python3 analysis/publication_pipeline.py normalize \
  --raw-run-dir logs/raw/sim-lte-3-static-none-firstpair/20260413T133000Z \
  --normalized-root logs/normalized

python3 analysis/publication_pipeline.py normalize \
  --raw-run-dir logs/raw/field-lte-3-static-none-bridgeproxy/20260413T133000Z \
  --normalized-root logs/normalized
```

Outputs:

- `logs/normalized/sim-lte-3-static-none-firstpair/20260413T133000Z/events.csv`
- `logs/normalized/sim-lte-3-static-none-firstpair/20260413T133000Z/dataset_manifest.json`
- `logs/normalized/field-lte-3-static-none-bridgeproxy/20260413T133000Z/events.csv`
- `logs/normalized/field-lte-3-static-none-bridgeproxy/20260413T133000Z/dataset_manifest.json`

Observed manifests:

- sim dataset:
  - `row_count = 6`
  - `sources = ["ns3_export"]`
- companion dataset:
  - `row_count = 30`
  - `sources = ["gcs", "rpi_bridge"]`

### Summarize

Commands:

```bash
python3 analysis/publication_pipeline.py summarize \
  --normalized-run-dir logs/normalized/sim-lte-3-static-none-firstpair/20260413T133000Z \
  --analysis-root logs/analysis

python3 analysis/publication_pipeline.py summarize \
  --normalized-run-dir logs/normalized/field-lte-3-static-none-bridgeproxy/20260413T133000Z \
  --analysis-root logs/analysis
```

Outputs:

- `logs/analysis/sim-lte-3-static-none-firstpair/20260413T133000Z/run_summary.json`
- `logs/analysis/sim-lte-3-static-none-firstpair/20260413T133000Z/run_summary.csv`
- `logs/analysis/sim-lte-3-static-none-firstpair/20260413T133000Z/metric_summary.csv`
- `logs/analysis/sim-lte-3-static-none-firstpair/20260413T133000Z/telemetry_continuity.csv`
- `logs/analysis/sim-lte-3-static-none-firstpair/20260413T133000Z/publication_table.md`
- `logs/analysis/field-lte-3-static-none-bridgeproxy/20260413T133000Z/run_summary.json`
- `logs/analysis/field-lte-3-static-none-bridgeproxy/20260413T133000Z/run_summary.csv`
- `logs/analysis/field-lte-3-static-none-bridgeproxy/20260413T133000Z/metric_summary.csv`
- `logs/analysis/field-lte-3-static-none-bridgeproxy/20260413T133000Z/telemetry_continuity.csv`
- `logs/analysis/field-lte-3-static-none-bridgeproxy/20260413T133000Z/publication_table.md`

Observed representative metrics:

Simulation export summary:

- `mean_rtt_ms = 21.643768499999997`
- `p95_rtt_ms = 27.204188000000002`
- `mean_jitter_ms = 0.012500000000000002`
- `mean_packet_loss_pct = 0.0`
- `mean_throughput_mbps = 0.009608166666666666`

Companion bridge summary:

- `mean_packet_loss_pct = 1.0666666666666667`

The companion run intentionally does not claim simulator-export RTT or throughput metrics. Its honest overlap with the simulator export is packet loss.

### Compare

Command:

```bash
python3 analysis/publication_pipeline.py compare \
  --field-summary logs/analysis/field-lte-3-static-none-bridgeproxy/20260413T133000Z/run_summary.json \
  --sim-summary logs/analysis/sim-lte-3-static-none-firstpair/20260413T133000Z/run_summary.json \
  --output-dir logs/analysis/comparisons/first_matched_pair
```

Outputs:

- `logs/analysis/comparisons/first_matched_pair/comparison_summary.json`
- `logs/analysis/comparisons/first_matched_pair/comparison_metrics.csv`
- `logs/analysis/comparisons/first_matched_pair/comparison_table.md`

Observed comparison result:

- metric overlap count: `1`
- overlapping metric: `mean_packet_loss_pct`
- field value: `1.0666666666666667`
- sim value: `0.0`
- mean absolute error: `1.0666666666666667`
- mean absolute percentage error: `100.0`

### Incompatibility fixed during this pass

The controlled companion run produced packet loss in real `battery_sample` rows, while the simulator export summary produces packet loss in `network_sample` rows. The analysis layer previously only promoted packet loss from `network_sample` into representative metrics.

Fix applied:

- `analysis/publication_pipeline.py`

Change:

- added a narrow fallback so `summarize` uses `battery_sample.packet_loss_pct` when the preferred evidence layer has no `network_sample.packet_loss_pct`

This was the minimum change required to let a real controlled bridge proxy compare honestly against the real simulator export without weakening evidence-layer separation.

## 5. Remaining Blockers Before Full Calibration/Validation

The remaining blockers are now experimental rather than infrastructure-level.

- no real field hardware pair has been collected yet
- the current companion run is a controlled bridge proxy, not a field campaign run
- the rendered GCS path still needs valid map/style credentials on the target machine to remove the HTTP 401 startup caveat
- only one minimal pair exists; repeated-run collection has not started
- no hold-out scenario reservation exists yet
- final calibration and hold-out validation have not been executed

## 6. Final Recommendation

### Is the full sim-side evidence path now working?

Yes.

This pass verified:

- real `ns-3` export through the project script stack
- publication-layout artifact generation for the simulator export
- real rendered GCS startup in the desktop/OpenGL path
- real GCS and RPi publication logs in a controlled companion run
- `normalize`, `summarize`, and `compare` on the first matched pair

### Is the project ready for a real calibration/validation campaign?

Yes.

The infrastructure gap is now closed tightly enough to move into calibration/validation. The remaining work is no longer general bring-up. It is controlled data collection.

### What should the next prompt be?

- `calibration/validation campaign`

It should not yet be a broad field experiment campaign. The next step is to collect a small repeated matched set with calibration discipline and reserve a hold-out subset before scaling up.
