# Publication Data Model

## Paper Focus

The paper is scoped to one claim family:

- sim-to-real calibration and communication evaluation
- not a generic feature inventory of the GCS or bridge

Everything in the dataset structure should support:

- command lifecycle measurement
- telemetry continuity analysis
- network performance analysis
- field-vs-simulation comparison
- scaled fleet evaluation after calibration

## Canonical Event Schema

The canonical merged dataset uses one row per event with the following stable fields:

| Field | Meaning |
| --- | --- |
| `scenario_id` | Structured scenario family identifier |
| `run_id` | Unique execution identifier |
| `event_id` | Unique row identifier |
| `source` | `gcs`, `rpi_bridge`, `ns3_export`, `ns3_live_publisher`, or other explicit producer |
| `event_type` | Controlled vocabulary listed below |
| `uav_id` | Stable UAV identifier when applicable |
| `timestamp_utc` | UTC wall-clock timestamp if available |
| `timestamp_monotonic_ms` | Local monotonic timestamp for intra-run timing |
| `rat` | `lte` or `nr` |
| `security_profile` | `none`, `tls`, `wireguard`, or `openvpn` |
| `sequence_id` | Message or packet sequence for continuity analysis |
| `command_id` | Command lifecycle identifier |
| `latitude`, `longitude`, `altitude_m` | Position state when applicable |
| `rtt_ms`, `jitter_ms`, `packet_loss_pct`, `throughput_mbps` | Communication metrics |
| `rssi_dbm`, `rsrp_dbm`, `rsrq_db`, `sinr_db` | Radio metrics |
| `status` | Result or state label |
| `note` | Free-text qualifier |

The normalized dataset also carries extended columns used for evidence separation and traceability:

- `evidence_layer`
- `metric_origin`
- `command_name`
- `command_code`
- `message_name`
- `sim_time_s`
- `sync_method`
- `sync_offset_ms`
- `battery_current_ma`
- `flow_id`
- `flow_type`
- `channel`

## Event Taxonomy

Use this fixed vocabulary in the merged dataset:

- `command_tx`
- `command_rx`
- `command_ack`
- `telemetry_rx`
- `network_sample`
- `sim_snapshot`
- `sync_status`
- `battery_sample`
- `packet_forward`
- `error`
- `selection_context`

Interpretation rules:

- `command_tx`, `command_rx`, `command_ack` are the only events used for command lifecycle timing.
- `telemetry_rx` and `battery_sample` are used for continuity and inter-arrival analysis.
- `network_sample` is used for measured modem/network samples and exported `ns-3` flow-monitor metrics.
- `sim_snapshot` is live simulator state for operator context; it is not automatically treated as validated evidence.
- Run-level summaries must not mix `field_ground_truth` and `simulator_export` metrics when computing representative per-run results. `field-*` scenarios summarize `field_ground_truth`; `sim-*` scenarios summarize `simulator_export`.

## Evidence Layers

Every normalized row must belong to one of these evidence layers:

- `field_ground_truth`
- `simulator_export`
- `ui_visualization_only`
- `derived_postprocess`

Interpret them strictly:

### `field_ground_truth`

Use for:

- GCS command transmission events
- bridge-side command receive/ack events
- telemetry receive timing
- measured modem or bridge network samples

### `simulator_export`

Use for:

- `ns-3` flow-monitor CSV exports
- simulator metadata tied to exported metrics

### `ui_visualization_only`

Use for:

- live `ns-3` snapshot metrics rendered in the GCS
- bridge-side live snapshot conversions that are based on estimated snapshot values
- any metric whose primary role is operator visualization rather than validated export

### `derived_postprocess`

Use for:

- normalized summaries
- comparison tables
- calibration error metrics
- any metric computed after collection

## Metric Origin

Use `metric_origin` to separate:

- `direct_observation`
- `direct_measurement`
- `runtime_metadata`
- `flow_monitor`
- `snapshot_estimate`
- `derived_postprocess`

This is mandatory for scientific honesty:

- `flow_monitor` is exported simulator evidence
- `snapshot_estimate` is live simulator visualization support
- the security overlay is a transport-overhead model, not a CPU-accurate crypto execution model

## Dataset Layout

Use this directory structure:

```text
logs/
  raw/<scenario_id>/<run_id>/
    gcs_events.jsonl
    gcs_metadata.json
    rpi_bridge_events.jsonl
    rpi_bridge_metadata.json
    ns3_lte_flow_monitor.csv
    ns3_lte_metadata.json
    run_manifest.json
  normalized/<scenario_id>/<run_id>/
    events.csv
    dataset_manifest.json
    events.parquet               # optional if pyarrow is available
  analysis/<scenario_id>/<run_id>/
    run_summary.json
    run_summary.csv
    metric_summary.csv
    telemetry_continuity.csv
    publication_table.md
  analysis/comparisons/<field_run>__vs__<sim_run>/
    comparison_summary.json
    comparison_metrics.csv
    comparison_table.md
```

## Scenario Naming Convention

Use:

`<domain>-<rat>-<uavs>-<motion>-<security>-<variant>`

Where:

- `domain`: `field` or `sim`
- `rat`: `lte` or `nr`
- `uavs`: integer fleet size
- `motion`: `static` or `mob`
- `security`: `none`, `tls`, `wireguard`, `openvpn`
- `variant`: distance/route/pair/calibration label such as `d050`, `routeA`, `pairA`

Examples:

- `field-lte-1-static-none-d050`
- `field-lte-2-mob-wireguard-pairA`
- `sim-lte-2-mob-none-pairA`
- `sim-nr-50-mob-openvpn-b4`

## Synchronization Metadata

Every run must log one `sync_status` event and should also carry the same information in `run_manifest.json`:

- `sync_method`
- `sync_offset_ms` if known
- `note`

Minimum acceptable submission-quality timing story:

- disciplined NTP or equivalent
- offset estimate recorded in the manifest or sync event
- explicit statement if no sub-millisecond synchronization was available

## Field Manifest

Each raw run directory should contain `run_manifest.json` for metadata not observable directly from software:

- site and launch location
- weather summary
- UAV model and firmware
- modem/operator
- altitude and motion profile
- telemetry/control intervals
- synchronization notes

Use [analysis/templates/run_manifest.template.json](/home/boots/work/phd/GCSNetworkPlanner/analysis/templates/run_manifest.template.json) as the template.
