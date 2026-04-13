# Publication Readiness Checklist

## Instrumentation

- [x] GCS emits structured publication logs
- [x] RPi bridge emits structured publication logs
- [x] `ns-3` LTE/NR runs emit scenario/run-aware CSV and metadata
- [x] command lifecycle uses `command_id` and `sequence_id`
- [x] telemetry continuity can be computed from timestamps and sequence IDs
- [x] evidence layers distinguish field, simulator-exported, UI-only, and derived metrics

## Experiment Metadata

- [x] run directory layout is defined
- [x] scenario naming convention is defined
- [x] run-manifest template exists
- [x] synchronization metadata fields exist
- [ ] all field operators are using the manifest consistently

## Minimum Field Campaign

- [ ] 1-UAV static LTE baseline complete
- [ ] 1-UAV mobility LTE baseline complete
- [ ] 2-UAV concurrent LTE baseline complete
- [ ] at least one secure field configuration collected if feasible
- [ ] all raw logs archived under the structured layout

## Mirrored Simulation

- [ ] each field scenario has a mirrored `sim-*` run
- [ ] payload size, control interval, telemetry interval, route, and fleet size match the field setup
- [ ] `ns-3` exports stored in the same `scenario_id/run_id` pattern

## Calibration And Validation

- [ ] calibration variables selected and documented
- [ ] mirrored runs used for calibration are labeled
- [ ] hold-out scenarios are reserved before tuning
- [ ] comparison pipeline executed on the hold-out set
- [ ] validation error tables produced

## Analysis Outputs

- [x] normalization pipeline exists
- [x] per-run summary pipeline exists
- [x] field-vs-sim comparison pipeline exists
- [ ] publication tables generated from real runs
- [ ] figures generated from final datasets

## Claim Discipline

- [x] UI-only metrics are labeled as such
- [x] simulator export and snapshot estimate are separated
- [x] security overlay limitation is documented
- [ ] manuscript text uses only validated evidence in the Results section

## Final Stop Rule

Do not draft final results/conclusion text until:

- [ ] three field scenario families are complete
- [ ] mirrored simulations exist
- [ ] hold-out validation has been run
- [ ] limitations and claim boundaries are written in the manuscript
