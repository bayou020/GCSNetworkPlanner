# Reproducibility Guide

## 1. Choose Run Metadata

For every run, define:

- `NP_SCENARIO_ID`
- `NP_RUN_ID`
- `NP_RAT`
- `NP_SECURITY_PROFILE`
- `NP_EXECUTION_MODE`
- `NP_SYNC_METHOD`
- optional `NP_SYNC_OFFSET_MS`
- optional `NP_SYNC_NOTE`
- optional `NP_RUN_NOTE`

Example:

```bash
export NP_SCENARIO_ID="sim-lte-2-mob-none-pairA"
export NP_RUN_ID="$(date -u +%Y%m%dT%H%M%SZ)"
export NP_RAT="lte"
export NP_SECURITY_PROFILE="none"
export NP_EXECUTION_MODE="real_hardware_field"
export NP_SYNC_METHOD="ntp"
```

Use one of:

- `real_hardware_field`
- `controlled_bridge_proxy`
- `hybrid_verifier`
- `pure_simulator`

## 2. Launch Components With Shared Metadata

Launch the GCS in the same shell or with the same exported variables:

```bash
./scripts/run.sh
```

Launch the RPi bridge for live `ns-3` mirroring:

```bash
../NetworkPlannerRpi/scripts/run_ns3_live.sh
```

Launch the simulator. The helper scripts now default to the structured run directory under `logs/raw/<scenario_id>/<run_id>/`.

LTE:

```bash
UAVS=2 SECURITY=none MOBILITY=1 ./sim/ns3/scripts/run_live_uav_lte_with_rpi.sh
```

NR:

```bash
UAVS=2 SECURITY=none MOBILITY=1 ./sim/ns3/scripts/run_live_uav_nr_with_rpi.sh
```

## 3. Archive Raw Artifacts

After a run, confirm the raw directory contains the expected files:

```bash
find "logs/raw/$NP_SCENARIO_ID/$NP_RUN_ID" -maxdepth 1 -type f | sort
```

If the run is a field run, place `run_manifest.json` in the same directory using:

- [analysis/templates/run_manifest.template.json](/home/boots/work/phd/GCSNetworkPlanner/analysis/templates/run_manifest.template.json)

Future mirrored simulator runs should contain both:

- `ns3_*_flow_monitor.csv`
- `ns3_*_link_model.csv`

Missing `ns3_*_link_model.csv` is now visible in readiness reports and blocks publication readiness for matched simulator comparisons.

## 4. Normalize Raw Logs

```bash
python3 analysis/publication_pipeline.py normalize \
  --raw-run-dir "logs/raw/$NP_SCENARIO_ID/$NP_RUN_ID"
```

Optional Parquet output:

```bash
python3 analysis/publication_pipeline.py normalize \
  --raw-run-dir "logs/raw/$NP_SCENARIO_ID/$NP_RUN_ID" \
  --write-parquet
```

The normalization manifest now records a pipeline fingerprint. If a normalized output predates the current pipeline, later stages treat it as stale and require a rebuild from raw logs.

## 5. Generate Per-Run Summaries

```bash
python3 analysis/publication_pipeline.py summarize \
  --normalized-run-dir "logs/normalized/$NP_SCENARIO_ID/$NP_RUN_ID"
```

This writes:

- `run_summary.json`
- `run_summary.csv`
- `metric_summary.csv`
- `telemetry_continuity.csv`
- `publication_table.md`

The summarizer refuses to reuse normalized outputs whose pipeline fingerprint is stale or missing.

## 6. Compare Field And Simulation Runs

```bash
python3 analysis/publication_pipeline.py compare \
  --field-summary logs/analysis/field-lte-2-mob-none-pairA/<field_run>/run_summary.json \
  --sim-summary logs/analysis/sim-lte-2-mob-none-pairA/<sim_run>/run_summary.json
```

The comparer likewise requires current run summaries and fails fast on stale summary artifacts.

## 6a. Rebuild Derived Outputs

To rebuild normalized outputs, per-run summaries, pair comparisons, and campaign outputs from raw logs:

```bash
python3 analysis/rebuild_publication_outputs.py \
  --campaign-config analysis/templates/small_calibration_campaign.r1.json
```

## 7. What To Use In The Paper

Use as evidence:

- `field_ground_truth` rows
- `simulator_export` rows
- `derived_postprocess` comparison outputs

Do not use as primary evidence:

- `ui_visualization_only` rows unless they are independently corroborated by simulator exports

Controlled proxy or hybrid verifier runs remain useful for development shakeout, but they are not substitutes for real field ground truth in publication mode.

## 8. Build/Environment Caveat

The analysis pipeline is independent of Qt at runtime, but collecting new raw data still depends on a working Qt 6.11 environment for the GCS and RPi binaries. If build verification is blocked by the local toolchain, collect logs on the machine that matches the intended experiment environment and run normalization/analysis on any Python 3 host.
