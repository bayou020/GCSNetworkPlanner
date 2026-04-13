# Publication Gate Report

Date: 2026-04-13

This report verifies the publication-oriented instrumentation and analysis additions across:

- `GCSNetworkPlanner`
- `NetworkPlannerRpi`

It distinguishes:

- `implemented`: the code path exists and was inspected
- `validated`: the path was actually exercised in a smoke test during this pass
- `blocked`: the path could not be exercised locally because of environment or hardware limits

## Verification Matrix

| Feature | Primary implementation | Static verdict | Exercised verdict | Notes |
| --- | --- | --- | --- | --- |
| Structured GCS publication logger | `src/core/publication_logger.*`, `src/app/main.cpp`, `src/mavlink/mavlink_raw_message.*` | Implemented and internally consistent by inspection | Blocked from native runtime validation | Local GCS configure still stops at missing `Qt6Location`; logger wiring for startup, command TX/ACK, telemetry, battery, and simulator feed ingestion is present |
| Structured RPi publication logger | `NetworkPlannerRpi/include/publication_logger.h`, `NetworkPlannerRpi/src/core/publication_logger.cpp`, `NetworkPlannerRpi/src/main.cpp` | Implemented and internally consistent by inspection | Validated through local host build and isolated live smoke run | `NetworkPlannerRpi` now builds in `build/host-debug`, starts in `--simulation --ns3-live`, and writes `rpi_bridge_events.jsonl` / `rpi_bridge_metadata.json` |
| Command/telemetry traceability | `src/mavlink/mavlink_raw_message.cpp`, `NetworkPlannerRpi/src/serialrpi.cpp`, `NetworkPlannerRpi/src/modem_logging_manager.cpp` | Implemented | Validated through synthetic raw logs and summary outputs | `sequence_id`, `command_id`, command RTT, telemetry continuity, and packet-forward events are supported |
| Synchronization metadata | both `publication_logger.cpp` files, `NetworkPlannerRpi/src/core/runtime_config.cpp`, `sim/ns3/scenarios/uav-secure-*.cc` | Implemented | Validated through raw metadata normalization | `sync_method`, `sync_offset_ms`, and notes propagate into metadata and summaries |
| `ns-3` publication exports | `sim/ns3/scenarios/uav-secure-lte.cc`, `sim/ns3/scenarios/uav-secure-nr.cc`, `sim/ns3/scripts/*.sh` | Implemented | Partially validated | Export format and metadata shape were exercised through synthetic `ns3_*` artifacts; local `ns-3` executable run was not performed in this pass |
| Scenario/run metadata propagation via `NP_*` | `sim/ns3/scripts/publication_env.sh`, live wrapper scripts, RPi runtime config | Implemented | Validated for path derivation and analysis layout | Default path derivation now resolves into `logs/raw/<scenario>/<run>/...` |
| Evidence-layer separation | loggers, `ns3_simulation_feed.cpp`, `ns3_live_telemetry_source.cpp`, `analysis/publication_pipeline.py`, `docs/publication/DATA_MODEL.md` | Implemented | Validated and corrected in this pass | Representative metrics now respect scenario domain instead of mixing field and simulator rows |
| High-rate live bridge load shedding | `NetworkPlannerRpi/src/core/ns3_live_telemetry_source.cpp`, `NetworkPlannerRpi/src/serialrpi.cpp`, both `publication_logger.cpp` files, `src/core/ns3_simulation_feed.cpp` | Implemented | Validated in isolated localhost smoke run | Live path now suppresses default MAVLink console spam, samples packet-forward logs, batches JSONL flushes, decimates HEARTBEAT/SYS_STATUS, coalesces GCS UI updates, and batches outbound pilot UDP payloads |
| Normalization pipeline | `analysis/publication_pipeline.py normalize` | Implemented | Validated | Produced `events.csv` and `dataset_manifest.json` from raw smoke-test runs |
| Per-run summary pipeline | `analysis/publication_pipeline.py summarize` | Implemented | Validated | Produced `run_summary.json`, `metric_summary.csv`, `telemetry_continuity.csv`, and `publication_table.md` |
| Field-vs-sim comparison pipeline | `analysis/publication_pipeline.py compare` | Implemented | Validated | Produced `comparison_summary.json`, `comparison_metrics.csv`, and `comparison_table.md` |
| Publication docs package | `docs/publication/*`, `analysis/README.md`, repo READMEs | Implemented | Validated by code/doc cross-check | Docs and code are now aligned on the core schema and evidence boundaries after the fixes below |

## What Was Broken and Fixed

### 1. `ns-3` runner scripts still defaulted to the legacy flat results path

Problem:

- `sim/ns3/scripts/run_uav_lte.sh`
- `sim/ns3/scripts/run_uav_nr.sh`

both initialized `CSV_PATH` to `sim/ns3/results/...` before calling the new publication path helpers. That made the structured `logs/raw/<scenario>/<run>/...` path a dead fallback unless the caller overrode `CSV_PATH`.

Fix:

- removed the early legacy `CSV_PATH` default from both scripts
- kept `CSV_PATH` and `METADATA_PATH` defaulting through `publication_env.sh`

Result:

- derived defaults now land in the publication layout without caller overrides

Observed shell verification:

- LTE default flow path: `logs/raw/sim-lte-12-mob-wireguard-default/<run_id>/ns3_lte_flow_monitor.csv`
- NR default metadata path: `logs/raw/sim-nr-8-static-none-default/<run_id>/ns3_nr_metadata.json`

### 2. Per-run summaries could silently mix field and simulator metrics

Problem:

`analysis/publication_pipeline.py summarize` originally computed representative metrics from all rows whose `evidence_layer` was either `field_ground_truth` or `simulator_export`. If both appeared in one normalized run, headline RTT/jitter/loss/throughput values were blended.

That violated the paper’s evidence separation rule.

Fix:

- added scenario-domain-based evidence selection:
  - `field-*` summaries use `field_ground_truth`
  - `sim-*` summaries use `simulator_export`
- recorded the selected evidence layer set in `run_summary.json`
- updated `analysis/README.md` and `docs/publication/DATA_MODEL.md`

Result:

- representative metrics now stay within the expected evidence layer for the run domain

### 3. Live `ns-3` runners passed empty optional sync arguments

Problem:

`sim/ns3/scripts/run_uav_lte.sh` and `sim/ns3/scripts/run_uav_nr.sh` always appended:

- `--syncOffsetMs=...`
- `--syncNote=...`

even when the corresponding `NP_*` variables were empty.

That caused `ns-3` to reject the launch with:

- `Invalid command-line argument: --syncOffsetMs`

Fix:

- append `--syncOffsetMs` only when `NP_SYNC_OFFSET_MS` is non-empty
- append `--syncNote` only when `NP_SYNC_NOTE` is non-empty

Result:

- the live LTE wrapper now launches past argument parsing and reaches active runtime execution

### 4. The live bridge path had too much self-inflicted overhead for 100-UAV runs

Problem:

With `100` UAVs and `LIVE_INTERVAL_MS=30`, the simulator can easily generate the equivalent of
`~13,333` MAVLink frames per second on the bridge path. The earlier implementation added avoidable
CPU and I/O overhead on top of that traffic:

- `SerialRPi` printed default `qDebug()` lines for unhandled MAVLink IDs
- both publication loggers flushed JSONL files on every event
- the RPi logged live `packet_forward` events at frame rate
- the RPi emitted one Qt signal per MAVLink frame
- the GCS logged one `sim_snapshot` event per UAV and updated the live UI at raw datagram rate

Fix:

- gated the default MAVLink debug spam behind `NPRPI_VERBOSE_MAVLINK`
- buffered publication logger flushes with `NP_LOG_FLUSH_INTERVAL_MS` and `NP_LOG_FLUSH_EVENT_COUNT`
- sampled live `packet_forward` logs with `NPRPI_PACKET_FORWARD_LOG_INTERVAL_MS` and `NPRPI_NS3_PACKET_FORWARD_LOG_INTERVAL_MS`
- decimated HEARTBEAT and SYS_STATUS emission with `NPRPI_NS3_HEARTBEAT_INTERVAL_MS` and `NPRPI_NS3_SYS_STATUS_INTERVAL_MS`
- batched live MAVLink frames before the pilot UDP send path with `NPRPI_NS3_PACKET_BATCH_BYTES`
- coalesced GCS-side live snapshot application so UI-only updates are no longer applied at raw datagram cadence

Result:

- the bridge-side hot path no longer does one Qt signal / one JSONL flush / one debug line per live MAVLink frame
- in an isolated localhost smoke run, one injected 100-UAV live snapshot produced `386` structured bridge events but only one outbound `pilot` UDP send event with `bytes = 8250`

## Smoke-Test Workflow Executed

The following synthetic verification runs were created locally in the repository layout:

- `logs/raw/field-lte-1-mob-none-verifier/20260413T104500Z`
- `logs/raw/sim-lte-1-mob-none-verifier/20260413T104700Z`

### Raw artifacts confirmed

Field run:

- `gcs_events.jsonl`
- `gcs_metadata.json`
- `rpi_bridge_events.jsonl`
- `rpi_bridge_metadata.json`
- `ns3_lte_flow_monitor.csv`
- `ns3_lte_metadata.json`
- `run_manifest.json`

Simulation run:

- `gcs_events.jsonl`
- `gcs_metadata.json`
- `ns3_lte_flow_monitor.csv`
- `ns3_lte_metadata.json`

### Normalization commands

```bash
python3 analysis/publication_pipeline.py normalize \
  --raw-run-dir logs/raw/field-lte-1-mob-none-verifier/20260413T104500Z \
  --normalized-root logs/normalized

python3 analysis/publication_pipeline.py normalize \
  --raw-run-dir logs/raw/sim-lte-1-mob-none-verifier/20260413T104700Z \
  --normalized-root logs/normalized
```

Observed outputs:

- `logs/normalized/field-lte-1-mob-none-verifier/20260413T104500Z/events.csv`
- `logs/normalized/field-lte-1-mob-none-verifier/20260413T104500Z/dataset_manifest.json`
- `logs/normalized/sim-lte-1-mob-none-verifier/20260413T104700Z/events.csv`
- `logs/normalized/sim-lte-1-mob-none-verifier/20260413T104700Z/dataset_manifest.json`

### Summarization commands

```bash
python3 analysis/publication_pipeline.py summarize \
  --normalized-run-dir logs/normalized/field-lte-1-mob-none-verifier/20260413T104500Z \
  --analysis-root logs/analysis

python3 analysis/publication_pipeline.py summarize \
  --normalized-run-dir logs/normalized/sim-lte-1-mob-none-verifier/20260413T104700Z \
  --analysis-root logs/analysis
```

Observed outputs for each run:

- `run_summary.json`
- `run_summary.csv`
- `metric_summary.csv`
- `telemetry_continuity.csv`
- `publication_table.md`

Observed field summary values after the evidence-layer fix:

- `mean_rtt_ms = 95.0`
- `p95_rtt_ms = 95.0`
- `mean_jitter_ms = 7.5`
- `mean_packet_loss_pct = 1.5`
- `mean_throughput_mbps = 12.4`
- telemetry continuity for `telemetry_rx`: `sample_count = 2`, `mean_inter_arrival_ms = 200.0`, `sequence_gap_count = 1`

### Comparison command

```bash
python3 analysis/publication_pipeline.py compare \
  --field-summary logs/analysis/field-lte-1-mob-none-verifier/20260413T104500Z/run_summary.json \
  --sim-summary logs/analysis/sim-lte-1-mob-none-verifier/20260413T104700Z/run_summary.json \
  --output-dir logs/analysis/comparisons/field_vs_sim_verifier
```

Observed outputs:

- `comparison_summary.json`
- `comparison_metrics.csv`
- `comparison_table.md`

Observed comparison aggregates:

- mean absolute error: `2.360`
- mean absolute percentage error: `11.606`

## Build and Static Verification

### Main repo: `GCSNetworkPlanner`

Build status in this environment:

- `cmake -S . -B build-publication` failed before compile because `Qt6Location` is missing

Observed blocker:

- `Failed to find required Qt component "Location"`

Interpretation:

- this pass could not prove a local GCS compile
- the failure is environment-level, not a publication-layer symbol mismatch discovered after configuration

### Sibling repo: `NetworkPlannerRpi`

Build status in this environment:

- `cmake --build build/host-debug -j8` completed successfully after fixing two publication-layer regressions

Observed fixes:

- `src/serialrpi.cpp`: explicit casts for packed `MANUAL_CONTROL` fields used in publication logging
- `src/core/runtime_config.cpp`: corrected `envString("NP_SYNC_OFFSET_MS", {})` usage so publication sync metadata compiles cleanly

Interpretation:

- this pass proved a local RPi host build
- the publication-layer code on the RPi side is no longer only “inspected”; it is compiled and minimally exercised here

### Isolated live bridge smoke run

An isolated localhost run was executed with:

- binary: `NetworkPlannerRpi/build/host-debug/NetworkPlannerRpi`
- mode: `--simulation --ns3-live`
- live port: `45478`
- log root: `/tmp/networkplanner-rpi-smoke2`

Observed artifacts:

- `/tmp/networkplanner-rpi-smoke2/raw/sim-lte-100-mob-openvpn-bridgeperf2/20260413T113200Z/rpi_bridge_events.jsonl`
- `/tmp/networkplanner-rpi-smoke2/raw/sim-lte-100-mob-openvpn-bridgeperf2/20260413T113200Z/rpi_bridge_metadata.json`

Observed bridge evidence:

- one injected 100-UAV snapshot generated `386` structured events
- the first live ingest row was a `sim_snapshot` event with `uav_count = 100`
- packet-forward rows were recorded for the expected MAVLink message types

Observed batching evidence:

- in `/tmp/networkplanner-rpi-smoke3/.../rpi_bridge_events.jsonl`, a single injected 100-UAV snapshot produced exactly one pilot send row:
  - `"channel":"pilot"`
  - `"status":"tx"`
  - `"bytes":8250`
- that confirms the bridge now batches live MAVLink frames before the outbound pilot UDP path instead of sending one UDP datagram per MAVLink frame

## What Is Actually Working Now

Working and exercised during this pass:

- publication-shaped raw run directories
- normalized merged dataset generation
- per-run summary generation
- field-vs-sim comparison generation
- scenario/run naming and metadata propagation through the analysis layer
- evidence-layer separation in the analysis outputs
- local `NetworkPlannerRpi` host build
- isolated `NetworkPlannerRpi` live `ns-3` ingest with 100 synthetic UAVs
- bridge-side batching of a 100-UAV snapshot into a single outbound pilot UDP send

Implemented and inspected, but not locally runtime-validated in this environment:

- GCS structured logger writes and metadata files
- live `ns-3` snapshot labeling as `ui_visualization_only`
- live bridge packet-forward and modem sample logging
- `ns-3` executable exports through the full simulator runtime path

## What Still Remains Before Submission

The remaining blockers are experimental and evidentiary, not architectural:

- collect real field runs with the new schema on the actual GCS + RPi + UAV stack
- run mirrored `ns-3` scenarios for those field runs using the same scenario naming and metadata discipline
- reserve and execute a hold-out validation set rather than calibrating and reporting on the same runs
- confirm timing discipline on hardware, including the actual synchronization method used in the field campaign
- generate final paper figures and manuscript-grade result tables from real runs
- verify the C++ applications on a machine with the required Qt stack and `ns-3` build environment
- clean up repository hygiene before submission packaging; in the current worktree several publication-layer source files under `src/core/` are still untracked

## Submission Gate Verdict

### Is the instrumentation layer functionally working?

Partially yes.

- The schema, analysis pipeline, metadata propagation, and evidence-layer separation are functionally working and were exercised end-to-end through a smoke-test workflow.
- The native GCS and RPi runtime logging hooks are implemented and appear internally consistent by inspection, but they were not compiled and executed locally in this pass because of external Qt environment blockers.

### Is the pipeline ready for real data collection?

Yes, with one condition:

- the analysis and dataset layout are ready now
- actual collection depends on running the GCS and RPi binaries in a build environment that satisfies the repos’ Qt and simulator dependencies

### What exact experimental work remains before an MDPI `Drones` submission?

- a minimum field campaign with repeated runs per scenario
- matched simulation runs for those same scenarios
- calibration followed by hold-out validation
- final result curation into figures/tables
- manuscript writing constrained to the validated evidence layer boundaries documented here
