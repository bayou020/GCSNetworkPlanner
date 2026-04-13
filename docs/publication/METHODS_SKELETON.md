# Methods Skeleton

## System Architecture

The evaluation platform combines three layers: a desktop Qt/QML ground control station, a Raspberry Pi companion bridge, and an `ns-3` LTE/NR simulation workspace. The GCS provides MAVLink command transmission, telemetry decoding, and live visualization. The Raspberry Pi service bridges serial or UDP MAVLink traffic, records modem/network samples, and can translate live `ns-3` snapshots into MAVLink telemetry. The simulator provides both live per-UAV snapshot streams for operator context and exported flow-monitor metrics for quantitative analysis.

## Measurement And Logging Methodology

All components write structured, per-run raw artifacts organized by `scenario_id` and `run_id`. Each event carries stable identifiers, UTC and monotonic time fields, source labels, and optional `command_id` or `sequence_id` fields for traceability. The logging design separates evidence layers explicitly: direct field observations are tagged as `field_ground_truth`, exported simulator metrics as `simulator_export`, live visualization-only metrics as `ui_visualization_only`, and post-processed outputs as `derived_postprocess`. Command lifecycle latency is measured from `command_tx` to visible `command_ack` events, telemetry continuity is derived from `telemetry_rx` and `battery_sample` timing and sequence gaps, and network summaries are computed from modem samples and simulator exports.

## Field Experiment Design

Field experiments are organized around repeatable scenario families defined by radio access technology, UAV count, motion profile, security profile, and variant label. The minimum publishable set includes one-UAV static LTE baselines, one-UAV mobility runs, and two-UAV concurrent runs, each repeated five times. For each run, a manifest records the site, weather, hardware stack, flight profile, payload rates, and timing synchronization notes. These field runs provide the real data used both for baseline characterization and for simulation calibration.

## Simulation Calibration And Validation

For each field scenario family, a mirrored `sim-*` scenario is configured in `ns-3` to match fleet size, motion mode, payload size, control interval, telemetry interval, and security profile. Calibration is performed only on a designated subset of mirrored runs by tuning delay, jitter, and loss-related assumptions while keeping the secure-overlay model constrained to transport overhead and setup delay. A separate hold-out set is reserved before tuning and is used to compute validation error metrics between field summaries and simulator summaries.

## Scaled Fleet Evaluation

After hold-out validation is acceptable, the calibrated simulation model is used for larger fleet sizes that are impractical to test physically. The scaled campaign evaluates LTE and NR under increasing UAV counts and security profiles while preserving the same core telemetry and control workloads used in the field. Results are reported as exported simulator metrics only, with the methodology making clear that the credibility of these larger studies depends on the preceding field calibration and validation steps.

## Limitations And Claim Boundaries

The study does not claim that live UI banner values are equivalent to validated simulator outputs; those values are used only for operator context unless corroborated by exported metrics. Likewise, the secure-tunnel model in `ns-3` captures transport overhead and setup delay rather than the full end-host CPU cost of cryptographic processing. If fine-grained timing synchronization better than disciplined NTP is not available in the field campaign, that limitation must be stated explicitly in the manuscript and reflected in the interpretation of absolute latency comparisons.
