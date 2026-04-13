# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

NetworkPlannerGCS is a Qt 6.11 / C++17 ground control station for UAV telemetry and network-aware field operations. It is a PhD research platform that integrates real-time map display, MAVLink/DJI vehicle control, ns-3 cellular simulation, and structured event logging for reproducible experiments.

## Build Commands

**Prerequisites:** Qt 6.11 (`$QT_ROOT` env var pointing to gcc_64 dir), MapLibre Native Qt (built via `./scripts/setup_maplibre_qt6.sh`), and Ubuntu system deps (`./scripts/setup_ubuntu_deps.sh`).

```bash
# First-time setup
cp env.example env         # fill in Mapbox token, OpenWeather key, etc.

# Configure (Ninja + RelWithDebInfo by default)
./scripts/configure.sh

# Build (uses nproc jobs by default; override with BUILD_JOBS=N)
./scripts/build.sh

# Run
source ./env && ./scripts/run.sh
```

Relevant env vars for configure/build:
- `QT_ROOT` — path to Qt 6.11 gcc_64 (default: `~/Qt/6.11.0/gcc_64`)
- `BUILD_DIR` — build output directory (default: `./build`)
- `MAPLIBRE_INSTALL_PREFIX` — MapLibre install dir (default: `.deps/maplibre-native-qt/install`)
- `CMAKE_BUILD_TYPE` — build type (default: `RelWithDebInfo`)

Binary output: `$BUILD_DIR/bin/NetworkPlannerGCS`

There is no automated test suite in the main codebase. Verification is done by running the application (smoke testing) and via live simulation workflows.

## ns-3 Simulation

```bash
# Install ns-3 (downloads ns-allinone-3.47 to .deps/)
./sim/ns3/scripts/setup_ns3.sh
./sim/ns3/scripts/build_ns3.sh

# Live LTE simulation feeding GCS on udp://127.0.0.1:45454
UAVS=20 BASE_STATIONS=4 SIM_TIME=60 ./sim/ns3/scripts/run_live_uav_lte.sh

# Live NR (5G) simulation
./sim/ns3/scripts/run_live_uav_nr.sh

# With Raspberry Pi bridge + video
./sim/ns3/scripts/run_live_uav_lte_with_rpi.sh
./sim/ns3/scripts/run_live_uav_nr_with_rpi.sh
```

Simulation env vars: `UAVS`, `BASE_STATIONS`, `SIM_TIME`, `SECURITY` (none/tls/wireguard/openvpn), `MOBILITY`.

Results CSVs land in `sim/ns3/results/`.

## Architecture

### Runtime Layers

| Layer | Location | Role |
|---|---|---|
| UI | `qml/screens/`, `qml/maps/`, `qml/components/` | QML reactive views |
| App/Service | `src/app/main.cpp` | Startup wiring; all C++ singletons registered as QML context properties here |
| Simulation | `src/core/ns3_simulation_feed.{h,cpp}` | Receives ns-3 UDP snapshots (port from env), drives map UAV markers and network metrics |
| Video | `src/core/video_stream_feed.{h,cpp}` | Receives per-UAV JPEG frames over UDP from RPi; tracks FPS/drop rates |
| Telemetry/Control | `src/mavlink/`, `src/network/` | MAVLink parsing, serial input, UDP transport, modem decode |
| DJI | `src/dji/` | Isolated DJI Onboard SDK wrapper |
| Publication | `src/core/publication_logger.{h,cpp}` | Thread-safe structured event logging (JSON) for reproducible paper experiments |

### Signal Flow

1. **Incoming:** Serial/UDP telemetry → `MavlinkRawMessage` → decoded values emitted as Qt signals → QML properties updated (position, attitude, battery, flight mode)
2. **Simulation:** ns-3 sends UDP snapshots → `Ns3SimulationFeed` parses JSON → emits `uavsChanged`, `antennasChanged` → `UavMap.qml` updates markers and `NetworkBanner` shows per-UAV metrics
3. **Control:** QML buttons/joystick → `MavGcsManager` / `DjiGcs` → UDP/serial out; backend is selected at runtime (MAVLink vs DJI) by checking active vehicle type
4. **Video:** RPi streams JPEG frames → `VideoStreamFeed` → `VideoMonitor.qml` displays selected UAV's feed

### Key Design Points

- **All C++ services are registered as QML context properties in `src/app/main.cpp`** — this is the single place to look for how C++ and QML are wired together (~50+ signal/slot connections).
- **Backend-agnostic control:** A runtime check (invoked from QML) selects DJI vs MAVLink; both share joystick input path.
- **QRC asset packaging:** QML files and icons are bundled via `resources/qml.qrc` and `resources/assets.qrc`. QRC aliases keep runtime URLs stable.
- **`legacy/`** contains Qt 5 widget-era code retained for reference only — it is not compiled.
- **`third_party/`** contains vendored MAVLink (v1+v2 generated C headers), DJI SDK, QCustomPlot, QFI flight instruments, and QJoysticks.

### ns-3 Simulation Scenarios

- `sim/ns3/scenarios/uav-secure-lte.cc` — 4G LTE fleet simulation
- `sim/ns3/scenarios/uav-secure-nr.cc` — 5G NR fleet simulation

Both send live UDP position+metric snapshots to `127.0.0.1:45454` for the GCS to consume. Batch (non-live) runs produce flow-level CSV metrics.

### Publication Logging

`PublicationLogger` (singleton, thread-safe) writes structured JSONL event logs during experiments. Configured via env vars: `SCENARIO_ID`, `RUN_ID`, `RAT` (LTE/NR), `SECURITY`. The `analysis/` directory contains Python pipeline scripts to process these logs for paper figures.

## Companion Repository: NetworkPlannerRpi

The Raspberry Pi onboard service lives at `../NetworkPlannerRpi/`. It is the embedded half of the platform — a headless `QCoreApplication` that bridges hardware (serial, modem, gimbal) and simulation feeds to the GCS over UDP.

**Network interface between the two repos:**

| Channel | RPi bind | GCS remote | Protocol |
|---|---|---|---|
| Pilot (MAVLink telemetry/commands) | `10.8.0.66:14552` | `10.8.0.62:14551` | MAVLink UDP |
| Modem (AT cmds / cellular metrics) | `10.8.0.66:14582` | `10.8.0.62:14581` | UDP |
| Gimbal control | `10.8.0.66:14592` | `10.8.0.62:14591` | UDP |
| Video (JPEG frames) | — | `127.0.0.1:5600` | GStreamer/UDP |
| ns-3 snapshots → GCS | — | `127.0.0.1:45454` | JSON/UDP |
| ns-3 snapshots → RPi | `127.0.0.1:45455` | — | JSON/UDP |

**RPi operating modes** (selected via env vars):
- Real hardware: serial pilot + modem AT + gimbal
- Pure simulation (`NPRPI_SIMULATION=1`): all hardware stubbed
- SITL bridge: forwards UDP from local PX4/ArduPilot
- MAVLink replay: plays back packets from a hex-encoded file
- ns-3 live: converts ns-3 fleet snapshots to MAVLink heartbeats + streams GStreamer video

Both repos share `PublicationLogger` (same JSONL format). Cross-repo env vars must align: `NP_SCENARIO_ID`/`SCENARIO_ID`, `NP_RUN_ID`/`RUN_ID`, `NP_RAT`/`RAT`, `NP_SECURITY_PROFILE`/`SECURITY`.
