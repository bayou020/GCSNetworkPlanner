# Experiment Execution Plan

## Goal

Produce the minimum defensible dataset for:

`A Sim-to-Real Cross-Layer Testbed for Cellular UAV Command and Telemetry Evaluation`

## Phase 0: Before First Flight

For every campaign day:

1. Choose the target `scenario_id`.
2. Generate a fresh `run_id`.
3. Fill `run_manifest.json` before takeoff.
4. Export the same `NP_*` metadata into all processes that will produce logs.
5. Confirm GCS, RPi, and `ns-3` raw outputs all point into the same run directory when the run is mirrored.

Required metadata per run:

- `scenario_id`
- `run_id`
- `rat`
- `security_profile`
- UAV count
- motion profile
- telemetry interval
- control interval
- altitude
- site
- weather summary
- sync method and offset note

## Minimum Publishable Run Set

### Field Calibration Set

Run each scenario `5` times:

1. `field-lte-1-static-none-d050`
2. `field-lte-1-static-none-d200`
3. `field-lte-1-mob-none-routeA`
4. `field-lte-2-static-none-pairA`
5. `field-lte-2-mob-none-pairA`

If secure field runs are feasible, add:

6. `field-lte-1-mob-wireguard-routeA`
7. `field-lte-2-mob-wireguard-pairA`

### Mirrored Simulation Set

Create one mirrored `sim-*` family for every field family:

1. `sim-lte-1-static-none-d050`
2. `sim-lte-1-static-none-d200`
3. `sim-lte-1-mob-none-routeA`
4. `sim-lte-2-static-none-pairA`
5. `sim-lte-2-mob-none-pairA`

If secure field runs exist, mirror those too.

### Hold-Out Validation Set

Reserve these before calibration:

1. one 1-UAV static distance variant
2. one 1-UAV mobility variant
3. one 2-UAV variant

These runs must not be used to tune:

- delay floor
- jitter model
- path-loss assumptions
- security overhead assumptions

## Scaled Simulation Set

After hold-out validation is acceptable:

1. `sim-lte-10-mob-none-scaleA`
2. `sim-lte-25-mob-none-scaleA`
3. `sim-lte-50-mob-none-scaleA`
4. `sim-lte-100-mob-none-scaleA`
5. `sim-nr-10-mob-none-scaleA`
6. `sim-nr-25-mob-none-scaleA`
7. `sim-nr-50-mob-none-scaleA`
8. `sim-nr-100-mob-none-scaleA`

Then add security sweeps:

- `none`
- `tls`
- `wireguard`
- `openvpn`

## Per-Run Procedure

1. Launch the GCS with the target `NP_*` metadata.
2. Launch the RPi bridge with the same `NP_*` metadata.
3. Launch the field run or mirrored `ns-3` scenario.
4. Execute the route or static dwell exactly as specified.
5. Stop the run cleanly and archive the raw run directory immediately.
6. Run normalization and summary generation before the next day’s experiments.

## Expected Outputs Per Run

Field run minimum:

- `gcs_events.jsonl`
- `gcs_metadata.json`
- `rpi_bridge_events.jsonl`
- `rpi_bridge_metadata.json`
- `run_manifest.json`

Mirrored simulation minimum:

- `ns3_lte_flow_monitor.csv` or `ns3_nr_flow_monitor.csv`
- matching `ns3_*_metadata.json`
- optional live snapshot logs from GCS/RPi when the live bridge is used

Derived outputs per run:

- normalized `events.csv`
- `run_summary.json`
- `metric_summary.csv`
- `telemetry_continuity.csv`

Derived outputs per comparison pair:

- `comparison_summary.json`
- `comparison_metrics.csv`
- `comparison_table.md`

## Stop Rule Before Writing Results

Do not freeze the Results section until:

1. calibration runs are complete
2. hold-out validation exists
3. comparison tables exist for RTT, jitter, loss, and throughput
4. evidence-layer audit confirms no UI-only metric is used as ground truth
