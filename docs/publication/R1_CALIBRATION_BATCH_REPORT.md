# R1 Calibration Batch Report

Date: 2026-04-13

This report covers only the first `r1` calibration batch:

- `cal-d050-r1`
- `cal-routeA-r1`
- `cal-pairA-r1`

No `r2`, `r3`, or hold-out slots were populated in this pass.

## 1. Run Inventory

All three `r1` field-side slots were completed as controlled bridge proxies using the real `GCSNetworkPlanner` and `NetworkPlannerRpi` binaries. No real field hardware was available in this environment.

| Pair | Field scenario | Field run | Field side type | Sim scenario | Sim run | Field artifacts | Sim artifacts | Comparison |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `cal-d050-r1` | `field-lte-1-static-none-d050` | `20260413T053029Z` | controlled bridge proxy | `sim-lte-1-static-none-d050` | `20260413T053029Z` | GCS + RPi + manifest | LTE export CSV + metadata | succeeded |
| `cal-routeA-r1` | `field-lte-1-mob-none-routeA` | `20260413T053030Z` | controlled bridge proxy | `sim-lte-1-mob-none-routeA` | `20260413T053030Z` | GCS + RPi + manifest | LTE export CSV + metadata | succeeded |
| `cal-pairA-r1` | `field-lte-2-static-none-pairA` | `20260413T053031Z` | controlled bridge proxy | `sim-lte-2-static-none-pairA` | `20260413T053031Z` | GCS + RPi + manifest | LTE export CSV + metadata | succeeded |

### Raw artifact inventory

For each completed field-side run:

- `gcs_events.jsonl`
- `gcs_metadata.json`
- `rpi_bridge_events.jsonl`
- `rpi_bridge_metadata.json`
- `run_manifest.json`

For each completed sim-side run:

- `ns3_lte_flow_monitor.csv`
- `ns3_lte_metadata.json`

## 2. Campaign Ledger Update

The reusable blank template was preserved. A campaign-specific working copy was created instead:

- `analysis/templates/small_calibration_campaign.r1.json`

Reason:

- the original file is a reusable template
- the working copy records the actual `r1` run IDs without turning the template itself into mutable campaign state

Only these slots were populated:

- `cal-d050-r1`
- `cal-routeA-r1`
- `cal-pairA-r1`

All other slots remain blank by design.

## 3. Pipeline Outputs

### Pair-level outputs

#### `cal-d050-r1`

Raw:

- `logs/raw/field-lte-1-static-none-d050/20260413T053029Z`
- `logs/raw/sim-lte-1-static-none-d050/20260413T053029Z`

Normalized:

- `logs/normalized/field-lte-1-static-none-d050/20260413T053029Z/events.csv`
- `logs/normalized/field-lte-1-static-none-d050/20260413T053029Z/dataset_manifest.json`
- `logs/normalized/sim-lte-1-static-none-d050/20260413T053029Z/events.csv`
- `logs/normalized/sim-lte-1-static-none-d050/20260413T053029Z/dataset_manifest.json`

Summaries:

- `logs/analysis/field-lte-1-static-none-d050/20260413T053029Z/run_summary.json`
- `logs/analysis/sim-lte-1-static-none-d050/20260413T053029Z/run_summary.json`

Comparison:

- `logs/analysis/comparisons/cal-d050-r1/comparison_summary.json`
- `logs/analysis/comparisons/cal-d050-r1/comparison_metrics.csv`
- `logs/analysis/comparisons/cal-d050-r1/comparison_table.md`

#### `cal-routeA-r1`

Raw:

- `logs/raw/field-lte-1-mob-none-routeA/20260413T053030Z`
- `logs/raw/sim-lte-1-mob-none-routeA/20260413T053030Z`

Normalized:

- `logs/normalized/field-lte-1-mob-none-routeA/20260413T053030Z/events.csv`
- `logs/normalized/field-lte-1-mob-none-routeA/20260413T053030Z/dataset_manifest.json`
- `logs/normalized/sim-lte-1-mob-none-routeA/20260413T053030Z/events.csv`
- `logs/normalized/sim-lte-1-mob-none-routeA/20260413T053030Z/dataset_manifest.json`

Summaries:

- `logs/analysis/field-lte-1-mob-none-routeA/20260413T053030Z/run_summary.json`
- `logs/analysis/sim-lte-1-mob-none-routeA/20260413T053030Z/run_summary.json`

Comparison:

- `logs/analysis/comparisons/cal-routeA-r1/comparison_summary.json`
- `logs/analysis/comparisons/cal-routeA-r1/comparison_metrics.csv`
- `logs/analysis/comparisons/cal-routeA-r1/comparison_table.md`

#### `cal-pairA-r1`

Raw:

- `logs/raw/field-lte-2-static-none-pairA/20260413T053031Z`
- `logs/raw/sim-lte-2-static-none-pairA/20260413T053031Z`

Normalized:

- `logs/normalized/field-lte-2-static-none-pairA/20260413T053031Z/events.csv`
- `logs/normalized/field-lte-2-static-none-pairA/20260413T053031Z/dataset_manifest.json`
- `logs/normalized/sim-lte-2-static-none-pairA/20260413T053031Z/events.csv`
- `logs/normalized/sim-lte-2-static-none-pairA/20260413T053031Z/dataset_manifest.json`

Summaries:

- `logs/analysis/field-lte-2-static-none-pairA/20260413T053031Z/run_summary.json`
- `logs/analysis/sim-lte-2-static-none-pairA/20260413T053031Z/run_summary.json`

Comparison:

- `logs/analysis/comparisons/cal-pairA-r1/comparison_summary.json`
- `logs/analysis/comparisons/cal-pairA-r1/comparison_metrics.csv`
- `logs/analysis/comparisons/cal-pairA-r1/comparison_table.md`

### Campaign-level outputs

Working config:

- `analysis/templates/small_calibration_campaign.r1.json`

Campaign pipeline output root:

- `logs/analysis/campaigns/small-calibration-lte-v1-r1`

Generated outputs:

- `logs/analysis/campaigns/small-calibration-lte-v1-r1/pair_inventory.csv`
- `logs/analysis/campaigns/small-calibration-lte-v1-r1/calibration_table.csv`
- `logs/analysis/campaigns/small-calibration-lte-v1-r1/calibration_table.md`
- `logs/analysis/campaigns/small-calibration-lte-v1-r1/holdout_validation_table.csv`
- `logs/analysis/campaigns/small-calibration-lte-v1-r1/holdout_validation_table.md`
- `logs/analysis/campaigns/small-calibration-lte-v1-r1/data_quality_report.json`
- `logs/analysis/campaigns/small-calibration-lte-v1-r1/data_quality_report.md`
- `logs/analysis/campaigns/small-calibration-lte-v1-r1/scale_up_recommendation.json`
- `logs/analysis/campaigns/small-calibration-lte-v1-r1/scale_up_recommendation.md`

## 4. Data Quality Findings

### `cal-d050-r1`

- raw artifacts exist: yes
- normalized outputs exist: yes
- summaries exist: yes
- comparison exists: yes
- field evidence-layer status: `field_ground_truth_rows = 29`
- sim evidence-layer status: `simulator_export_rows = 2`
- metric overlap count: `1`
- overlapping metric: `mean_packet_loss_pct`
- caveat: controlled bridge proxy, not real field hardware

### `cal-routeA-r1`

- raw artifacts exist: yes
- normalized outputs exist: yes
- summaries exist: yes
- comparison exists: yes
- field evidence-layer status: `field_ground_truth_rows = 38`
- sim evidence-layer status: `simulator_export_rows = 2`
- metric overlap count: `1`
- overlapping metric: `mean_packet_loss_pct`
- caveat: controlled bridge proxy, not real field hardware

### `cal-pairA-r1`

- raw artifacts exist: yes
- normalized outputs exist: yes
- summaries exist: yes
- comparison exists: yes
- field evidence-layer status: `field_ground_truth_rows = 53`
- sim evidence-layer status: `simulator_export_rows = 4`
- metric overlap count: `1`
- overlapping metric: `mean_packet_loss_pct`
- caveat: controlled bridge proxy, not real field hardware

### Batch-wide findings

Observed campaign status from the working ledger:

- total configured pairs: `18`
- complete pairs: `3`
- pending pairs: `15`
- failed pairs: `0`
- missing raw pairs: `0`

Observed campaign recommendation:

- `status = not_ready`
- `scale_up_recommended = false`

Reasons:

1. only the three `r1` slots are intentionally complete
2. hold-out slots are still reserved and empty
3. each completed pair is below the configured minimum metric overlap gate of `3`

## 5. Workflow Notes

One workflow issue had to be fixed before `cal-routeA-r1` was valid:

- `sim/ns3/scripts/run_uav_lte.sh`

Fix:

- pass `--mobility` and `--mobilityRadius` even when `LIVE=0`

Without that fix, a non-live `sim-lte-1-mob-none-routeA` export run would have been mislabeled as mobility while actually remaining static.

## 6. r2 Readiness Answer

### Should `r2` proceed now?

No.

### Why not?

The `r1` batch is structurally complete, but the completed pairs only overlap on one comparison metric:

- `mean_packet_loss_pct`

That is not enough to justify continuing calibration repetitions as if the comparison path were already strong.

### What exact issue must be fixed first?

At least one stronger field-side comparison path must be established before `r2`:

- preferably one true field hardware run
- or, if hardware remains unavailable, a targeted improvement that produces additional field-side representative metrics beyond packet loss, especially RTT/jitter/throughput-aligned evidence

Until then, `r2` would mostly repeat controlled proxy runs that do not add meaningful calibration depth.

### Recommended next prompt

- `targeted fix pass`

That fix pass should focus on improving the field-side evidence richness for calibration, not on expanding the campaign size.
