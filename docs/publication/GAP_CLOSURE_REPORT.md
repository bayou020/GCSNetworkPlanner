# Gap Closure Report

## Scope

This report tracks the exact publication blockers for the MDPI `Drones` submission path and maps them to code, scripts, or documentation now present in the repositories.

## Closed Or Scaffolded Gaps

| Gap | Severity | Type | Closure Status | Evidence |
| --- | --- | --- | --- | --- |
| No unified logging schema across GCS, RPi, and simulator | must fix | code/implementation | closed at instrumentation level | structured JSONL logger in GCS and RPi; structured `ns-3` metadata/export paths; [DATA_MODEL.md](./DATA_MODEL.md) |
| No stable scenario/run identifiers | must fix | code/implementation | closed | `NP_SCENARIO_ID`, `NP_RUN_ID`, structured raw log directories, updated `ns-3` scripts |
| No command traceability | must fix | code/implementation | partially closed | GCS now logs `command_tx`, matched `command_ack`, RTT, `command_id`, `sequence_id`; RPi logs `command_rx` and bridge ACKs |
| No telemetry continuity instrumentation | must fix | code/implementation | partially closed | telemetry and battery events are now structured; summary pipeline computes inter-arrival and sequence-gap statistics |
| UI metrics mixed with evidence | must fix | writing/claim discipline | closed in code/docs | `evidence_layer` and `metric_origin` separate `simulator_export` from `ui_visualization_only` |
| No reproducible raw/normalized/analysis dataset layout | must fix | dataset/reproducibility | closed | `logs/raw`, `logs/normalized`, `logs/analysis` pipeline plus run-manifest template |
| No analysis pipeline | must fix | analysis | scaffolded and runnable | `analysis/publication_pipeline.py` normalization, per-run summary, and field-vs-sim comparison |
| No explicit synchronization metadata | must fix | methodology | partially closed | `sync_status`, `sync_method`, optional `sync_offset_ms`, run-manifest timing block |
| Overclaim risk in security evaluation | must fix | writing/claim discipline | closed in docs, still needs manuscript discipline | simulator metadata and docs explicitly state overhead/setup-delay model only |

## Still Open Before Submission

| Gap | Severity | Type | Why It Remains |
| --- | --- | --- | --- |
| Real field dataset not yet collected in the new schema | must fix | dataset | code is ready, but the paper still needs real runs |
| Calibration and hold-out validation not yet executed | must fix | methodology/analysis | comparison scripts exist, but no final calibrated dataset is present |
| Journal figures/tables not yet populated with real data | must fix | analysis/writing | pipeline is ready; results are still missing |
| End-to-end clock discipline not yet validated in hardware | should fix | methodology | metadata can record it, but actual field timing quality still needs confirmation |
| Full build verification blocked by local Qt environment mismatch | should fix | reproducibility | this workstation lacks the exact Qt setup needed for both repos |

## What Changed In Code

### GCS

- structured publication logger added
- command transmission and acknowledgment logging added
- telemetry and battery receive events logged
- live `ns-3` snapshot ingestion now records provenance and marks snapshot values as visualization-only
- selected UAV context logging added

### Raspberry Pi Bridge

- structured publication logger added
- runtime config now carries scenario/run/RAT/security/sync metadata
- packet forwarding, bridge command receive, ACK emission, and modem samples are logged
- live `ns-3` bridge ingestion logs snapshot provenance and packet emission events

### Simulator

- `ns-3` LTE and NR scenarios now emit scenario/run-aware flow-monitor CSV and metadata JSON
- helper scripts now default to publication-style run directories instead of only flat result files
- live snapshot stream explicitly marks estimated metrics as `ui_visualization_only`

### Analysis/Docs

- normalization, summary, and comparison pipeline added
- publication data model, methods skeleton, execution plan, checklist, and reproducibility guide added

## What Is Now Measurable

- command ACK RTT when ACKs are visible at the GCS
- telemetry inter-arrival timing and sequence gaps
- packet forwarding event counts and bridge-side transfer traces
- modem/network sample summaries
- exported `ns-3` delay, jitter, throughput, and packet-loss summaries
- run-level field-vs-sim aggregate error metrics

## What Is Still Only Demo Or Operator Context

- live `ns-3` banner metrics rendered in the GCS
- live `ns-3` snapshot values used to shape simulated video impairment
- any result derived solely from `sim_snapshot` rows without a matching simulator export

## Submission Readiness Verdict

The repos are materially closer to publishability because the instrumentation and evidence-path gaps are now addressed at the software level.

The project is not yet submission-ready because the final paper still requires:

- field runs collected with the new schema
- mirrored simulation runs
- hold-out validation
- paper figures populated from actual data
