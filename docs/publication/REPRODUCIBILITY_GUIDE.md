# Reproducibility Guide

## 1. Choose Run Metadata

For every run, define:

- `NP_SCENARIO_ID`
- `NP_RUN_ID`
- `NP_RAT`
- `NP_SECURITY_PROFILE`
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
export NP_SYNC_METHOD="ntp"
```

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

## 6. Compare Field And Simulation Runs

```bash
python3 analysis/publication_pipeline.py compare \
  --field-summary logs/analysis/field-lte-2-mob-none-pairA/<field_run>/run_summary.json \
  --sim-summary logs/analysis/sim-lte-2-mob-none-pairA/<sim_run>/run_summary.json
```

## 7. What To Use In The Paper

Use as evidence:

- `field_ground_truth` rows
- `simulator_export` rows
- `derived_postprocess` comparison outputs

Do not use as primary evidence:

- `ui_visualization_only` rows unless they are independently corroborated by simulator exports

## 8. Build/Environment Caveat

The analysis pipeline is independent of Qt at runtime, but collecting new raw data still depends on a working Qt 6.11 environment for the GCS and RPi binaries. If build verification is blocked by the local toolchain, collect logs on the machine that matches the intended experiment environment and run normalization/analysis on any Python 3 host.
