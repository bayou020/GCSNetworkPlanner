# Collision Avoidance

This document defines the maintained collision-detection and avoidance layer used by `GCSNetworkPlanner` during live ns-3 fleet runs.

## Scope

The implementation is a GCS-side, rule-based safety layer.

It is designed to:

- detect unsafe pairwise UAV separation from live snapshot geometry
- classify proximity into `SAFE`, `WARNING`, and `ALERT`
- emit operator-visible warnings in the UI
- issue immediate MAVLink avoidance commands for `ALERT` cases
- generate structured logs suitable for publication analysis

It is not a certified onboard collision-avoidance system.

## Detection Policy

Default thresholds:

- `WARNING` enter: `< 10 m` 3D separation
- `WARNING` exit: `>= 12 m` 3D separation
- `ALERT` enter: `<= 6 m` 3D separation
- `ALERT` exit: `> 8 m` 3D separation
- safety-volume enter: `|north| <= 5 m`, `|east| <= 5 m`, `|vertical| <= 5 m`
- safety-volume exit: `|north| <= 6 m`, `|east| <= 6 m`, `|vertical| <= 6 m`

The `5 m x 5 m x 5 m` box is treated as a hard safety volume. Entering it raises `ALERT` immediately even if the pair has not crossed the 3D-distance threshold through some degenerate geometry.

Hysteresis is intentional. It prevents alert/warning flapping when a pair hovers near a threshold.

## Motion Metrics

The GCS computes per-UAV velocity estimates from consecutive live snapshots.

From those estimates, it derives per-pair motion metrics:

- horizontal separation
- vertical separation
- closing speed
- time to closest approach
- predicted minimum distance

These metrics are used for:

- richer operator context in the UI
- structured publication logging

The current rule-based alert trigger still uses explicit distance and safety-volume thresholds rather than a pure predictive trigger.

## Avoidance Action

When a pair is in `ALERT`, the GCS computes a repulsive avoidance vector for each UAV.

Current control outputs:

- horizontal separation through `roll/pitch`
- vertical separation through `throttle`
- zeroed command release when the alert clears

Commands are rate-limited and use a configurable floor/magnitude so the bridge does not oscillate between tiny ineffective corrections.

The GCS sends:

- MAVLink `MANUAL_CONTROL` for the immediate avoidance actuation
- MAVLink `COLLISION` for a standard advisory record of the threat

## Environment Variables

Use these variables to make the policy explicit in a run manifest or shell wrapper:

- `NP_GCS_COLLISION_WARNING_ENTER_M`
- `NP_GCS_COLLISION_WARNING_EXIT_M`
- `NP_GCS_COLLISION_ALERT_ENTER_M`
- `NP_GCS_COLLISION_ALERT_EXIT_M`
- `NP_GCS_COLLISION_SAFETY_AXIS_ENTER_M`
- `NP_GCS_COLLISION_SAFETY_AXIS_EXIT_M`
- `NP_GCS_COLLISION_COMMAND_INTERVAL_MS`
- `NP_GCS_COLLISION_COMMAND_MAGNITUDE`
- `NP_GCS_COLLISION_COMMAND_FLOOR`
- `NP_GCS_COLLISION_SEND_MAVLINK_REPORT`

## UI Exposure

The maintained UI exposes collision state in three places:

- map markers change visual emphasis for `WARNING` and `ALERT`
- a top-right collision banner announces the most severe active pair
- the right-side vehicle panel can display collision metrics such as:
  - severity
  - nearest peer
  - 3D separation
  - horizontal separation
  - vertical separation
  - closing speed
  - time to closest approach
  - predicted minimum distance
  - avoidance mode

## Publication Logs

The collision layer emits structured events in the GCS log:

- `collision_policy`
  - threshold and actuation configuration for the run
- `collision_state`
  - entered, changed, or cleared pair states
- `collision_avoidance`
  - avoidance actions actually commanded by the GCS
- `command_tx`
  - MAVLink `MANUAL_CONTROL`
- `collision_report_tx`
  - MAVLink `COLLISION`

The RPi bridge should log corresponding `command_rx` events for `MANUAL_CONTROL` when the pilot UDP bridge or equivalent inbound command path is active.

## Claim Boundary

Use disciplined wording in the paper:

- acceptable:
  - `rule-based collision monitoring`
  - `GCS-side collision avoidance layer`
  - `MAVLink-triggered separation commands`
  - `safety-volume breach detection`
- avoid without further validation:
  - `guaranteed collision prevention`
  - `certified detect-and-avoid`
  - `autonomous onboard sense-and-avoid`

The present layer is publication-grade for controlled experiments and operator-facing evaluation, not for aviation safety certification claims.
