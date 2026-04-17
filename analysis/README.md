# Publication Analysis Pipeline

This directory contains the minimum analysis path needed for the paper:

- normalize raw run artifacts into one merged dataset
- summarize command, telemetry, and network behavior per run
- compare one field run against one mirrored simulation run

## Commands

Normalize one run:

```bash
python3 analysis/publication_pipeline.py normalize \
  --raw-run-dir logs/raw/<scenario_id>/<run_id>
```

Summarize one normalized run:

```bash
python3 analysis/publication_pipeline.py summarize \
  --normalized-run-dir logs/normalized/<scenario_id>/<run_id>
```

The summarizer selects representative communication metrics from the evidence layer implied by the scenario domain:

- `field-*` scenarios use `field_ground_truth`
- `sim-*` scenarios use `simulator_export`

This prevents auxiliary artifacts from another layer from contaminating per-run headline metrics.

Compare one field summary against one simulation summary:

```bash
python3 analysis/publication_pipeline.py compare \
  --field-summary logs/analysis/<field_scenario>/<field_run>/run_summary.json \
  --sim-summary logs/analysis/<sim_scenario>/<sim_run>/run_summary.json
```

Batch a small calibration plus hold-out campaign:

```bash
python3 analysis/campaign_pipeline.py \
  --campaign-config analysis/templates/small_calibration_campaign.template.json
```

Rebuild derived outputs from raw logs and campaign configs:

```bash
python3 analysis/rebuild_publication_outputs.py \
  --campaign-config analysis/templates/small_calibration_campaign.r1.json
```

## Outputs

Normalization writes:

- `logs/normalized/<scenario_id>/<run_id>/events.csv`
- `logs/normalized/<scenario_id>/<run_id>/dataset_manifest.json`
- optional `events.parquet` if `--write-parquet` is requested and `pandas+pyarrow` are available

Each normalized dataset manifest now records:

- the normalization pipeline fingerprint and version
- raw artifact presence, including whether `ns3_*_flow_monitor.csv` and `ns3_*_link_model.csv` existed
- an explicit or inferred execution mode

Summarization writes:

- `logs/analysis/<scenario_id>/<run_id>/run_summary.json`
- `logs/analysis/<scenario_id>/<run_id>/run_summary.csv`
- `logs/analysis/<scenario_id>/<run_id>/metric_summary.csv`
- `logs/analysis/<scenario_id>/<run_id>/telemetry_continuity.csv`
- `logs/analysis/<scenario_id>/<run_id>/publication_table.md`

Comparison writes:

- `logs/analysis/comparisons/<field_run>__vs__<sim_run>/comparison_summary.json`
- `logs/analysis/comparisons/<field_run>__vs__<sim_run>/comparison_metrics.csv`
- `logs/analysis/comparisons/<field_run>__vs__<sim_run>/comparison_table.md`

Campaign batching writes:

- `logs/analysis/campaigns/<campaign_name>/pair_inventory.csv`
- `logs/analysis/campaigns/<campaign_name>/calibration_table.csv`
- `logs/analysis/campaigns/<campaign_name>/calibration_table.md`
- `logs/analysis/campaigns/<campaign_name>/holdout_validation_table.csv`
- `logs/analysis/campaigns/<campaign_name>/holdout_validation_table.md`
- `logs/analysis/campaigns/<campaign_name>/data_quality_report.json`
- `logs/analysis/campaigns/<campaign_name>/data_quality_report.md`
- `logs/analysis/campaigns/<campaign_name>/development_readiness.json`
- `logs/analysis/campaigns/<campaign_name>/development_readiness.md`
- `logs/analysis/campaigns/<campaign_name>/publication_readiness.json`
- `logs/analysis/campaigns/<campaign_name>/publication_readiness.md`
- `logs/analysis/campaigns/<campaign_name>/scale_up_recommendation.json`
- `logs/analysis/campaigns/<campaign_name>/scale_up_recommendation.md`

`scale_up_recommendation.*` stays publication-strict. Use `development_readiness.*` for partial-batch or shakeout status.

## Run Manifest

If a run directory contains `run_manifest.json`, the normalization step carries it into the dataset manifest.

Field manifests should set `execution.mode` to one of:

- `real_hardware_field`
- `controlled_bridge_proxy`
- `hybrid_verifier`

If older manifests do not contain that field, the pipeline infers it and records the inference source in the run summary.

Use [run_manifest.template.json](/home/boots/work/phd/GCSNetworkPlanner/analysis/templates/run_manifest.template.json) as the starting point for field campaigns.

Use [small_calibration_campaign.template.json](/home/boots/work/phd/GCSNetworkPlanner/analysis/templates/small_calibration_campaign.template.json) as the starting point for the first small calibration plus hold-out campaign.

Use [small_calibration_campaign.r1.json](/home/boots/work/phd/GCSNetworkPlanner/analysis/templates/small_calibration_campaign.r1.json) for the current `r1` shakeout batch. It emits both development and publication readiness, but defaults the config to the development profile.
