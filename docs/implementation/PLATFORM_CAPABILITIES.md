# Platform Capabilities

This document summarizes what the platform can do today across the three active parts of the system:

- `GCSNetworkPlanner` desktop ground station
- `NetworkPlannerRpi` Raspberry Pi bridge
- `sim/ns3` cellular simulation workspace

It is intentionally implementation-focused. It describes the workflows that are actually wired together now.

## 1. Ground Control Station

The desktop application currently provides:

- MapLibre Native Qt base map with Mapbox-hosted styles
- optional OpenWeather raster overlays and click-driven weather queries
- live multi-UAV visualization from ns-3
- antenna markers and coverage circles
- selected-UAV network banner
- selected-UAV HUD
- selected-UAV video monitor
- planner, joystick, and control menus inherited from the original platform

## 2. Vehicle Selection Model

The current selection flow is:

1. ns-3 publishes live UAV positions and network metrics
2. the map renders one marker per UAV
3. clicking a UAV selects it by ns-3 `id`
4. the selected UAV id is mapped to the mirrored MAVLink `sysid`
5. the UI merges:
   - network metrics from ns-3
   - vehicle state from the per-`sysid` MAVLink cache

That merged selection now drives:

- the right-side network banner
- the battery popup
- the top battery icon state
- the HUD
- the video monitor

## 3. MAVLink / DJI Control Path

The active command path supports:

- takeoff
- land
- RTL
- mode changes
- manual-control enable/disable
- selected-UAV joystick routing

Backend behavior:

- MAVLink vehicles use command-long or set-mode traffic
- DJI vehicles use the retained DJI bridge path

The control workflow is selection-aware. Manual control is intended to follow the currently selected UAV.

## 4. Raspberry Pi Bridge Modes

`NetworkPlannerRpi` now supports multiple runtime modes:

- hardware-backed bridge mode
- localhost simulation mode
- MAVLink replay mode
- SITL UDP bridge mode
- live ns-3 mirrored telemetry mode
- simulated video emission mode

In the integrated live stack, the RPi bridge:

- consumes mirrored ns-3 snapshots
- emits MAVLink `HEARTBEAT`
- emits `SYS_STATUS`
- emits GPS packets
- emits `ATTITUDE`
- emits simulated video frames whose delay/drop behavior follows the live network metrics

## 5. Cellular Simulation Stack

The maintained local experiment matrix currently covers:

- LTE
- NR

The local `ns-allinone-3.47` bundle does not provide:

- GSM
- UMTS

Live ns-3 runs can provide:

- projected GPS coordinates
- UAV mobility
- per-UAV ping
- RSSI
- RSRP
- RSRQ
- SINR
- jitter
- packet loss
- throughput
- serving cell
- distance to serving antenna

## 6. Video Impairment Model

The video stack is designed for user-visible degradation studies.

Current design:

- GStreamer generates a synthetic live source in `NetworkPlannerRpi`
- frames are JPEG-encoded
- the sender fans out per-UAV datagrams
- live ns-3 metrics influence drop and delay
- the GCS displays the selected UAV stream

This is not a full media-network simulation stack. It is a practical impairment model for studying visible outcomes under different radio and tunnel conditions.

## 7. End-to-End Workflows

### Desktop GCS only

Use this when validating the UI, map stack, and local controls.

### GCS + live ns-3

Use this when validating:

- live UAV motion
- per-UAV network metrics
- antenna coverage rendering

### GCS + live ns-3 + RPi bridge

Use this when validating:

- per-UAV MAVLink state mirroring
- selected-UAV telemetry in the UI
- manual-control selection flow
- video degradation behavior

## 8. Current Limits

- The platform still contains legacy UI and vendor debt from the earlier codebase.
- Some widgets still use prototype-era interaction patterns.
- The video model is metric-shaped and not packet-for-packet media transport through ns-3.
- The simulated vehicle state can briefly lag the map markers at startup until the per-UAV MAVLink cache warms up.
- Some shutdown paths still need cleanup and stability work.

## 9. Recommended Reading

- [README.md](/home/boots/work/phd/GCSNetworkPlanner/README.md)
- [docs/implementation/BUILD.md](/home/boots/work/phd/GCSNetworkPlanner/docs/implementation/BUILD.md)
- [docs/implementation/CONFIGURATION.md](/home/boots/work/phd/GCSNetworkPlanner/docs/implementation/CONFIGURATION.md)
- [docs/implementation/VIDEO_SIMULATION.md](/home/boots/work/phd/GCSNetworkPlanner/docs/implementation/VIDEO_SIMULATION.md)
- [sim/ns3/README.md](/home/boots/work/phd/GCSNetworkPlanner/sim/ns3/README.md)
