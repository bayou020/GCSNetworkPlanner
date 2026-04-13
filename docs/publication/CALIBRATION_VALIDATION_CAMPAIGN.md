# Small Calibration And Hold-Out Validation Campaign

Date: 2026-04-13

This document narrows the next step after infrastructure bring-up into one concrete campaign:

- small enough to execute without overcommitting hardware time
- strict enough to reserve hold-out before tuning
- structured enough to produce calibration, hold-out, and data-quality outputs after each batch

## Phase 1: First Minimal Families

Use these initial field families:

### Calibration families

1. `field-lte-1-static-none-d050`
2. `field-lte-1-mob-none-routeA`
3. `field-lte-2-static-none-pairA`

Use these mirrored simulator families:

1. `sim-lte-1-static-none-d050`
2. `sim-lte-1-mob-none-routeA`
3. `sim-lte-2-static-none-pairA`

## Phase 2: Reserve Hold-Out Before Tuning

Reserve these families before any simulator tuning:

### Static hold-out

- `field-lte-1-static-none-d200`
- `sim-lte-1-static-none-d200`

### Mobility hold-out

- `field-lte-1-mob-none-routeB`
- `sim-lte-1-mob-none-routeB`

### Two-UAV hold-out

- `field-lte-2-static-none-pairB`
- `sim-lte-2-static-none-pairB`

These hold-out families must not be used to tune:

- delay floor
- jitter assumptions
- loss assumptions
- security overhead assumptions

## Phase 3: First Repeated Set

Start with `3` repetitions per family.

That gives:

- `3` calibration families
- `3` hold-out families
- `3` repetitions each
- `18` field-vs-sim pairs total

Do not expand to `5+` repetitions until:

- artifact generation is stable
- the data-quality report is clean
- the hold-out tables are populated

## Phase 4: After Each Batch

For every new completed pair:

1. normalize the field run
2. summarize the field run
3. normalize the mirrored sim run
4. summarize the mirrored sim run
5. compare the pair

Then regenerate campaign-level outputs:

- calibration table
- hold-out validation table
- data quality report
- scale-up recommendation

## Phase 5: Outputs

The campaign must produce:

- calibration table
- hold-out validation table
- data quality report
- recommendation on whether to scale up

## Campaign Config

Use [small_calibration_campaign.template.json](/home/boots/work/phd/GCSNetworkPlanner/analysis/templates/small_calibration_campaign.template.json) as the run ledger for this campaign.

It already includes:

- the `3` calibration families
- the `3` hold-out families
- `3` repetition slots per family
- conservative campaign quality gates

Fill the `field_run_id` and `sim_run_id` fields only after the raw runs exist.

## Batch Helper

Run the batch helper with:

```bash
python3 analysis/campaign_pipeline.py \
  --campaign-config analysis/templates/small_calibration_campaign.template.json
```

It writes campaign outputs under:

- `logs/analysis/campaigns/<campaign_name>/pair_inventory.csv`
- `logs/analysis/campaigns/<campaign_name>/calibration_table.csv`
- `logs/analysis/campaigns/<campaign_name>/calibration_table.md`
- `logs/analysis/campaigns/<campaign_name>/holdout_validation_table.csv`
- `logs/analysis/campaigns/<campaign_name>/holdout_validation_table.md`
- `logs/analysis/campaigns/<campaign_name>/data_quality_report.json`
- `logs/analysis/campaigns/<campaign_name>/data_quality_report.md`
- `logs/analysis/campaigns/<campaign_name>/scale_up_recommendation.json`
- `logs/analysis/campaigns/<campaign_name>/scale_up_recommendation.md`

The helper tolerates incomplete campaigns. Empty run slots remain `pending` and are surfaced in the data-quality report instead of crashing the batch.

## Recommended Operational Order

1. collect one full `r1` batch across all calibration families
2. collect one full mirrored `r1` sim batch
3. run the campaign helper
4. inspect the calibration table and data-quality report
5. repeat for `r2`
6. repeat for `r3`
7. only after calibration tuning is frozen, collect the reserved hold-out families
8. rerun the campaign helper and inspect the hold-out validation table

## Scale-Up Gate

Do not scale beyond this campaign until all of the following are true:

- all expected pairs in the configured batch are complete
- no pair is missing raw artifacts
- every hold-out pair has non-zero metric overlap
- no sim summary is missing `simulator_export` evidence
- no field summary is missing `field_ground_truth` evidence

If these conditions are met, move to the repeated calibration/validation campaign rather than another infrastructure pass.
