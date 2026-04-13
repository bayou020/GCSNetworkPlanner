# Native Bring-Up Report

Date: 2026-04-13

This report covers dependency bring-up, repo hygiene, native build verification, and native runtime verification for:

- `GCSNetworkPlanner`
- `NetworkPlannerRpi`

It does not restate the publication architecture. It focuses on what was required to make the current environment build and run the real binaries.

## 1. Dependency Resolution Status

### What was missing or misconfigured

#### GCS

- plain `cmake -S . -B ...` picked the system Qt `6.4.2`, which does not provide the intended `Qt6Location` configuration for this repo
- `QMapLibre` was not installed into the repo-local prefix expected by the build scripts

#### RPi

- plain `cmake -S . -B ...` also picked the system Qt `6.4.2`, while the repo requires Qt `6.11`
- the required GStreamer development libraries were already present on this machine, but they were not being reached until the Qt prefix issue was fixed

### What was installed or configured

- used the local Qt toolchain at:
  - `/home/boots/Qt/6.11.0/gcc_64`
- built and installed repo-local `QMapLibre` into:
  - `/home/boots/work/phd/GCSNetworkPlanner/.deps/maplibre-native-qt/install`
- configured both repos through their intended Qt 6.11 path instead of relaxing version requirements

### Versions verified in use

- `cmake 3.28.3`
- `pkg-config 1.8.1`
- `Qt 6.11.0`
- `SDL2 2.30.0`
- `gstreamer-1.0 1.24.2`
- `gstreamer-app-1.0 1.24.2`
- `gstreamer-video-1.0 1.24.2`
- `QMapLibre libQMapLibre.so.4`

### Version requirement changes

None.

The environment was brought up to the repo requirements. The Qt `6.11` requirement in `NetworkPlannerRpi` was not relaxed.

## 2. Repo Hygiene Status

### Source tracking fixes

#### GCS

- fixed `.gitignore` so source files under `src/core/` are no longer hidden by the stray `core` ignore pattern
- staged the publication-layer source and docs that should be tracked:
  - `analysis/*`
  - `docs/publication/*`
  - `sim/ns3/scripts/publication_env.sh`
  - `src/core/*`

#### RPi

- staged the publication logger source files that should be tracked:
  - `include/publication_logger.h`
  - `src/core/publication_logger.cpp`

### Generated artifact ignore policy

#### GCS `.gitignore`

Added ignores for:

- `/build/`
- `/build-*/`
- `/logs/`
- `/outputs/`
- `analysis/__pycache__/`
- `*.pyc`

#### RPi `.gitignore`

Added ignores for:

- `build/`
- `build-*/`
- `logs/`
- `outputs/`
- `__pycache__/`
- `*.pyc`

### Policy

- source code and durable publication docs are versioned
- generated runtime data is not versioned
- build trees are not versioned
- local analysis output directories are not versioned

## 3. Build Status

### GCS

#### Configure command

```bash
QT_ROOT="$HOME/Qt/6.11.0/gcc_64" \
MAPLIBRE_INSTALL_PREFIX="$PWD/.deps/maplibre-native-qt/install" \
BUILD_DIR="$PWD/build/host-verify" \
./scripts/configure.sh
```

#### Build command

```bash
cmake --build build/host-verify -j8
```

#### Result

Build passed.

#### Fixes applied during build bring-up

- built and installed repo-local `QMapLibre`
- fixed packed MAVLink field logging in:
  - `src/mavlink/mavlink_raw_message.cpp`

#### Publication code confirmed in build

Observed object builds included:

- `src/core/publication_logger.cpp.o`
- `src/core/ns3_simulation_feed.cpp.o`
- `src/mavlink/mavlink_raw_message.cpp.o`
- `src/app/main.cpp.o`

Final binary:

- `build/host-verify/bin/NetworkPlannerGCS`

### RPi

#### Configure command

```bash
QT_ROOT="$HOME/Qt/6.11.0/gcc_64" \
BUILD_DIR="$PWD/build/host-verify" \
./scripts/configure.sh
```

#### Build command

```bash
cmake --build build/host-verify -j8
```

#### Result

Build passed.

#### Fixes applied during bring-up

- earlier packed MAVLink field fix in `src/serialrpi.cpp`
- earlier runtime config fix in `src/core/runtime_config.cpp`
- timed buffered logger flush in `src/core/publication_logger.cpp`

#### Publication code confirmed in build

Observed object builds included:

- `src/core/publication_logger.cpp.o`
- `src/core/ns3_live_telemetry_source.cpp.o`
- `src/modem_link_manager.cpp.o`
- `src/serialrpi.cpp.o`

Final binary:

- `/home/boots/work/phd/NetworkPlannerRpi/build/host-verify/NetworkPlannerRpi`

## 4. Native Runtime Verification

### Workflow run

A shared native smoke run was executed with:

- scenario: `sim-lte-3-static-none-nativeverify2`
- run: `20260413T124500Z`
- log root: `/tmp/native-pipeline-verify2`

#### GCS

Headless launch:

```bash
env \
  NP_SCENARIO_ID=sim-lte-3-static-none-nativeverify2 \
  NP_RUN_ID=20260413T124500Z \
  NP_LOG_ROOT=/tmp/native-pipeline-verify2 \
  NP_RAT=lte \
  NP_SECURITY_PROFILE=none \
  NS3_SIM_PORT=45484 \
  NPGCS_PILOT_BIND_HOST=127.0.0.1 \
  NPGCS_PILOT_BIND_PORT=24551 \
  NPGCS_PILOT_REMOTE_HOST=127.0.0.1 \
  NPGCS_PILOT_REMOTE_PORT=24552 \
  NPGCS_MODEM_BIND_HOST=127.0.0.1 \
  NPGCS_MODEM_BIND_PORT=24581 \
  NPGCS_MODEM_REMOTE_HOST=127.0.0.1 \
  NPGCS_MODEM_REMOTE_PORT=24582 \
  NPGCS_GIMBAL_BIND_HOST=127.0.0.1 \
  NPGCS_GIMBAL_BIND_PORT=24591 \
  NPGCS_GIMBAL_REMOTE_HOST=127.0.0.1 \
  NPGCS_GIMBAL_REMOTE_PORT=24592 \
  QT_QPA_PLATFORM=offscreen \
  QT_QUICK_BACKEND=software \
  LIBGL_ALWAYS_SOFTWARE=1 \
  ./build/host-verify/bin/NetworkPlannerGCS
```

#### RPi

Native binary in simulation mode:

```bash
env \
  NP_SCENARIO_ID=sim-lte-3-static-none-nativeverify2 \
  NP_RUN_ID=20260413T124500Z \
  NP_LOG_ROOT=/tmp/native-pipeline-verify2 \
  NP_RAT=lte \
  NP_SECURITY_PROFILE=none \
  NPRPI_SIMULATION=1 \
  NPRPI_ENABLE_NS3_LIVE_TELEMETRY=1 \
  NPRPI_ENABLE_VIDEO_SIMULATION=0 \
  NPRPI_ENABLE_MODEM=0 \
  NPRPI_ENABLE_GIMBAL_SERIAL=0 \
  NPRPI_NS3_LIVE_PORT=45485 \
  NPRPI_PILOT_HOST=127.0.0.1 \
  NPRPI_PILOT_BIND_PORT=24552 \
  NPRPI_PILOT_REMOTE_PORT=24551 \
  NPRPI_MODEM_HOST=127.0.0.1 \
  NPRPI_MODEM_BIND_PORT=24582 \
  NPRPI_MODEM_REMOTE_PORT=24581 \
  NPRPI_GIMBAL_HOST=127.0.0.1 \
  NPRPI_GIMBAL_BIND_PORT=24592 \
  NPRPI_GIMBAL_REMOTE_PORT=24591 \
  NPRPI_PACKET_FORWARD_LOG_INTERVAL_MS=1000 \
  NPRPI_NS3_PACKET_FORWARD_LOG_INTERVAL_MS=1000 \
  NPRPI_NS3_PACKET_BATCH_BYTES=8192 \
  /home/boots/work/phd/NetworkPlannerRpi/build/host-verify/NetworkPlannerRpi \
  --simulation --ns3-live --ns3-port 45485
```

#### Runtime stimulus

One real UDP snapshot was injected into:

- GCS `Ns3SimulationFeed` on `127.0.0.1:45484`
- RPi `Ns3LiveTelemetrySource` on `127.0.0.1:45485`

The apps themselves generated all resulting publication log files.

### Artifacts produced by the real apps

Raw run directory:

- `/tmp/native-pipeline-verify2/raw/sim-lte-3-static-none-nativeverify2/20260413T124500Z`

Produced by the binaries:

- `gcs_events.jsonl`
- `gcs_metadata.json`
- `rpi_bridge_events.jsonl`
- `rpi_bridge_metadata.json`

### Example log rows

#### GCS

```json
{"event_id":"gcs-000002","event_type":"sim_snapshot","evidence_layer":"ui_visualization_only","message_name":"NS3_SNAPSHOT","metric_origin":"snapshot_estimate","rat":"lte","run_id":"20260413T124500Z","scenario_id":"sim-lte-3-static-none-nativeverify2","schema_version":1,"security_profile":"none","sim_time_s":2,"source":"gcs","status":"ingested","timestamp_monotonic_ms":18926,"timestamp_utc":"2026-04-13T04:33:02.265Z","uav_count":3}
{"event_id":"gcs-000004","event_type":"telemetry_rx","evidence_layer":"field_ground_truth","mavlink_msg_id":0,"message_name":"HEARTBEAT","metric_origin":"direct_observation","rat":"lte","run_id":"20260413T124500Z","scenario_id":"sim-lte-3-static-none-nativeverify2","schema_version":1,"security_profile":"none","sequence_id":0,"source":"gcs","status":"STANDBY","timestamp_monotonic_ms":18943,"timestamp_utc":"2026-04-13T04:33:02.282Z","uav_id":1,"vehicle_type":"QUADROTOR"}
```

#### RPi

```json
{"event_id":"rpi_bridge-000002","event_type":"sim_snapshot","evidence_layer":"ui_visualization_only","message_name":"NS3_LIVE_SNAPSHOT","metric_origin":"snapshot_estimate","rat":"lte","run_id":"20260413T124500Z","scenario_id":"sim-lte-3-static-none-nativeverify2","schema_version":1,"security_profile":"none","sim_time_s":2,"source":"rpi_bridge","status":"ingested","timestamp_monotonic_ms":12304,"timestamp_utc":"2026-04-13T04:33:02.264Z","uav_count":3}
{"bytes":64,"channel":"ns3_live_to_pilot","event_id":"rpi_bridge-000005","event_type":"packet_forward","evidence_layer":"field_ground_truth","message_name":"GPS_RAW_INT","metric_origin":"direct_observation","rat":"lte","run_id":"20260413T124500Z","sample_interval_ms":1000,"scenario_id":"sim-lte-3-static-none-nativeverify2","schema_version":1,"security_profile":"none","source":"rpi_bridge","status":"tx","timestamp_monotonic_ms":12304,"timestamp_utc":"2026-04-13T04:33:02.264Z","uav_id":1}
```

### Observed event counts

#### GCS

- `sync_status: 1`
- `sim_snapshot: 1`
- `selection_context: 1`
- `telemetry_rx: 9`
- `battery_sample: 3`

#### RPi

- `sync_status: 1`
- `sim_snapshot: 1`
- `packet_forward: 13`

### Analysis pipeline on real app-generated logs

Commands:

```bash
python3 analysis/publication_pipeline.py normalize \
  --raw-run-dir /tmp/native-pipeline-verify2/raw/sim-lte-3-static-none-nativeverify2/20260413T124500Z \
  --normalized-root /tmp/native-pipeline-verify2/normalized

python3 analysis/publication_pipeline.py summarize \
  --normalized-run-dir /tmp/native-pipeline-verify2/normalized/sim-lte-3-static-none-nativeverify2/20260413T124500Z \
  --analysis-root /tmp/native-pipeline-verify2/analysis
```

Result:

- `normalize` passed
- `summarize` passed

Outputs:

- `/tmp/native-pipeline-verify2/normalized/sim-lte-3-static-none-nativeverify2/20260413T124500Z/events.csv`
- `/tmp/native-pipeline-verify2/normalized/sim-lte-3-static-none-nativeverify2/20260413T124500Z/dataset_manifest.json`
- `/tmp/native-pipeline-verify2/analysis/sim-lte-3-static-none-nativeverify2/20260413T124500Z/run_summary.json`
- `/tmp/native-pipeline-verify2/analysis/sim-lte-3-static-none-nativeverify2/20260413T124500Z/run_summary.csv`
- `/tmp/native-pipeline-verify2/analysis/sim-lte-3-static-none-nativeverify2/20260413T124500Z/metric_summary.csv`
- `/tmp/native-pipeline-verify2/analysis/sim-lte-3-static-none-nativeverify2/20260413T124500Z/telemetry_continuity.csv`
- `/tmp/native-pipeline-verify2/analysis/sim-lte-3-static-none-nativeverify2/20260413T124500Z/publication_table.md`

### Runtime issues found and fixed in this pass

#### 1. GCS packed MAVLink logging broke native compile

File:

- `src/mavlink/mavlink_raw_message.cpp`

Fix:

- copied packed `MANUAL_CONTROL` fields into plain `int` values before inserting them into the publication logger payload

#### 2. Buffered loggers could leave low-volume bursts unflushed

Files:

- `src/core/publication_logger.h`
- `src/core/publication_logger.cpp`
- `/home/boots/work/phd/NetworkPlannerRpi/include/publication_logger.h`
- `/home/boots/work/phd/NetworkPlannerRpi/src/core/publication_logger.cpp`

Fix:

- kept buffered logging
- added scheduled timer-based flushes so logs become durable after the configured interval even if no later event arrives

Without this fix, the combined native smoke run only showed the first two rows while the apps were still running.

## 5. Remaining Blockers Before Real Experiment Collection

These are the blockers that still remain after build and native smoke bring-up.

### Still blocked or only partially validated

- no real `ns-3` export run was executed in this pass, so:
  - `ns3_*_flow_monitor.csv`
  - `ns3_*_metadata.json`
  remain unverified in the current environment
- GCS was validated headlessly with `QT_QPA_PLATFORM=offscreen`; this proves the logging and UDP ingest path, but not a full rendered desktop map workflow
- the headless GCS run still reports:
  - `QOpenGLContext is NULL`
  - `You are running on QSG backend "software"`
  which is expected for this offscreen smoke path but means the rendered MapLibre path still needs one smoke run on the intended desktop/OpenGL environment
- no real field hardware run was executed
- no calibration / hold-out validation set exists yet

### Verdict on readiness

#### Are both repos now buildable in the intended environment?

Yes, on this machine when configured against the local Qt `6.11.0` toolchain and the repo-local `QMapLibre` install.

#### Is the native publication logging path working?

Yes.

Both real binaries emitted publication logs, and the shared native smoke run produced:

- `gcs_events.jsonl`
- `gcs_metadata.json`
- `rpi_bridge_events.jsonl`
- `rpi_bridge_metadata.json`

The analysis pipeline also passed on those real app-generated logs.

#### Is the system ready for real field/simulation data collection?

Ready for controlled experiment bring-up, but not yet fully cleared for final collection.

The remaining engineering work before real collection is:

- one real `ns-3` export run that produces `simulator_export` artifacts under the publication schema
- one rendered GCS smoke on the actual desktop/OpenGL target environment
- then calibration / matched simulation runs

## 6. Final Recommendation

The next prompt should be:

- `calibration/validation`

Reason:

- dependency bring-up is complete
- native binary build is complete
- native publication logging is working
- the next high-value step is not more general cleanup
- the next step is to run one real `ns-3` export workflow plus a small calibration set, then hold-out validation
