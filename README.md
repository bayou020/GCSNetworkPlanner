# NetworkPlannerGCS

NetworkPlannerGCS is an open-source Qt 6.11 ground control station for UAV telemetry, map-centric mission planning, joystick-driven control workflows, and network-aware field operations. The application combines a QML front end with C++ services for MAVLink, DJI integration, MapLibre/Mapbox rendering, plotting, and OpenWeather overlays.

This repository is maintained as a public codebase with a reproducible build flow, explicit environment-based configuration, and a documented project layout.

## Status

- Maintained desktop build path: Ubuntu/Linux
- Primary UI stack: Qt Quick / QML
- Primary build system: CMake
- Primary map stack: MapLibre Native Qt with Mapbox styles
- Project maturity: functional and documented, with some legacy subsystem debt still being reduced

## Features

- Qt 6.11 desktop application with a QML-based flight UI
- MapLibre Native Qt integration with Mapbox-hosted styles
- Click-driven OpenWeather markers and optional weather raster overlays
- Live ns-3 LTE/NR simulation overlays for UAV and antenna visualization in QML
- Clickable live simulation UAVs with right-panel network metrics for LTE and NR studies
- Simulated per-UAV video monitor fed from the Raspberry Pi bridge and shaped by live ns-3 loss and delay
- MAVLink telemetry decoding and command dispatch
- DJI Onboard SDK integration retained from the earlier codebase
- SDL2-backed joystick support through QJoysticks
- QCustomPlot-based charting and flight-instrument overlays
- C++/QML bridge layer for telemetry, controls, plotting, and map interaction

## Platform Capabilities

The repository is no longer just a legacy GCS prototype. The current platform can run several end-to-end workflows that combine the desktop GCS, the Raspberry Pi bridge, and the ns-3 cellular simulation stack.

### 1. Desktop GCS

- interactive MapLibre/Mapbox base map with weather overlays
- live UAV selection on the map
- right-side network banner with per-UAV LTE or NR metrics
- HUD with heading, pitch, roll, and altitude
- manual-control workflow for the selected UAV
- command dispatch for takeoff, land, RTL, and flight-mode changes

### 2. Raspberry Pi Bridge Integration

- MAVLink telemetry decoding and command dispatch
- DJI backend retained for the original platform integration path
- localhost simulation and replay flows through `NetworkPlannerRpi`
- per-UAV mirrored MAVLink state from the live ns-3 stack
- per-UAV cached telemetry so selection-driven UI panels can show vehicle-specific state

### 3. Cellular Simulation Stack

- live ns-3 LTE and NR fleet simulation
- projected GPS positions for antennas and UAVs
- moving UAV markers on the map
- per-UAV network quality metrics:
  - `PING`
  - `RSSI`
  - `RSRP`
  - `RSRQ`
  - `SINR`
  - `JITTER`
  - `LOSS`
  - `THRPT`
  - serving cell and distance

### 4. Video Impairment Simulation

- `NetworkPlannerRpi` can emit a simulated GStreamer video stream
- the live ns-3 metrics shape frame delay and frame loss per UAV
- the GCS video monitor displays the selected UAV stream in real time
- you can study visible degradation under congestion, loss, and secure-tunnel overhead

### 5. Integrated Workflow

The maintained workflow is:

1. run the desktop GCS
2. run the live LTE or NR experiment
3. optionally run the integrated RPi bridge through the combined `*_with_rpi.sh` helpers
4. click a UAV on the map
5. inspect:
   - vehicle state
   - network metrics
   - HUD values
   - simulated video behavior

That gives you a reproducible way to exercise UAV control-plane behavior and user-visible degradation on one platform.

Detailed platform notes:

- [docs/implementation/PLATFORM_CAPABILITIES.md](/home/boots/work/phd/GCSNetworkPlanner/docs/implementation/PLATFORM_CAPABILITIES.md)

## Repository Highlights

- Clean domain-oriented source layout under `src/`
- QML separated into screens, map views, and reusable components
- Vendored dependencies isolated under `third_party/`
- Legacy Qt 5/widget-era code archived under `legacy/`
- Setup/configure/build/run scripts under `scripts/`
- `sim/ns3/` workspace for cellular UAV/GCS network experiments
- Dedicated docs for build, configuration, architecture, contribution, and troubleshooting

## Quick Start

1. Install Ubuntu system dependencies:

   ```bash
   ./scripts/setup_ubuntu_deps.sh
   ```

2. Install Qt 6.11 with the official Qt installer and export `QT_ROOT`:

   ```bash
   export QT_ROOT="$HOME/Qt/6.11.0/gcc_64"
   ```

3. Build and install MapLibre Native Qt locally:

   ```bash
   ./scripts/setup_maplibre_qt6.sh
   ```

4. Create your local environment file:

   ```bash
   cp env.example env
   ```

5. Configure, build, and run:

   ```bash
   ./scripts/configure.sh
   ./scripts/build.sh
   ./scripts/run.sh
   ```

## Common Workflows

### Run The Desktop GCS

```bash
source ./env
./scripts/run.sh
```

### Run Live LTE Fleet Simulation

```bash
source ./env
UAVS=20 BASE_STATIONS=4 SIM_TIME=60 LIVE_INTERVAL_MS=500 MOBILITY=1 \
NS3_LTE_TX_POWER=38 NS3_LTE_DL_BANDWIDTH=75 NS3_LTE_UL_BANDWIDTH=75 \
./sim/ns3/scripts/run_live_uav_lte.sh
```

### Run Live LTE + RPi + Video Stack

```bash
source ./env
UAVS=20 BASE_STATIONS=4 SIM_TIME=60 LIVE_INTERVAL_MS=500 MOBILITY=1 \
./sim/ns3/scripts/run_live_uav_lte_with_rpi.sh
```

### Run Live NR + RPi + Video Stack

```bash
source ./env
UAVS=20 BASE_STATIONS=4 SIM_TIME=60 LIVE_INTERVAL_MS=500 MOBILITY=1 \
NS3_NR_TX_POWER=46 NS3_NR_BANDWIDTH=80000000 NS3_NR_NUMEROLOGY=2 \
./sim/ns3/scripts/run_live_uav_nr_with_rpi.sh
```

### Stress The Video Link

```bash
source ./env
UAVS=100 BASE_STATIONS=2 SIM_TIME=90 LIVE_INTERVAL_MS=400 MOBILITY=1 SECURITY=openvpn \
NPRPI_VIDEO_WIDTH=640 NPRPI_VIDEO_HEIGHT=360 NPRPI_VIDEO_FPS=15 \
./sim/ns3/scripts/run_live_uav_lte_with_rpi.sh
```

## Configuration

Runtime configuration is driven by environment variables rather than hardcoded project-local secrets. The tracked template is [env.example](/home/boots/work/phd/GCSNetworkPlanner/env.example); your machine-local copy should live in `env`.

Core variables:

- `MAPBOX_ACCESS_TOKEN`
- `MAPBOX_STYLE_URL`
- `OPENWEATHERMAP_API_KEY`
- `OPENWEATHERMAP_TILE_LAYER`
- `QSG_RHI_BACKEND`
- `NS3_SIM_PORT`
- `NS3_SIM_RPI_PORT`
- `NS3_TELEMETRY_PAYLOAD`
- `NS3_TELEMETRY_INTERVAL_MS`
- `NS3_CONTROL_PAYLOAD`
- `NS3_CONTROL_INTERVAL_MS`
- `NS3_LTE_TX_POWER`
- `NS3_LTE_DL_BANDWIDTH`
- `NS3_LTE_UL_BANDWIDTH`
- `NS3_NR_TX_POWER`
- `NS3_NR_BANDWIDTH`
- `NS3_NR_FREQUENCY`
- `NS3_NR_NUMEROLOGY`
- `NPVIDEO_HOST`
- `NPVIDEO_PORT`

Full reference:

- [docs/implementation/CONFIGURATION.md](/home/boots/work/phd/GCSNetworkPlanner/docs/implementation/CONFIGURATION.md)
- [docs/implementation/VIDEO_SIMULATION.md](/home/boots/work/phd/GCSNetworkPlanner/docs/implementation/VIDEO_SIMULATION.md)
- [docs/implementation/TROUBLESHOOTING.md](/home/boots/work/phd/GCSNetworkPlanner/docs/implementation/TROUBLESHOOTING.md)

## Project Layout

```text
src/          First-party C++ application and service code
qml/          QML screens, map views, and reusable UI components
assets/       Icons and packaged runtime visuals
resources/    QRC manifests and Qt resource configuration
third_party/  Vendored dependencies preserved in-tree
legacy/       Archived code excluded from the active build
scripts/      Bootstrap, configure, build, and run automation
docs/implementation/  Build, architecture, configuration, troubleshooting, and contribution guides
```

Detailed layout notes:

- [docs/implementation/PROJECT_LAYOUT.md](/home/boots/work/phd/GCSNetworkPlanner/docs/implementation/PROJECT_LAYOUT.md)
- [docs/implementation/ARCHITECTURE.md](/home/boots/work/phd/GCSNetworkPlanner/docs/implementation/ARCHITECTURE.md)
- [sim/ns3/README.md](/home/boots/work/phd/GCSNetworkPlanner/sim/ns3/README.md)

## Documentation Index

- [Implementation Docs Index](/home/boots/work/phd/GCSNetworkPlanner/docs/implementation/README.md)
- [Build Guide](/home/boots/work/phd/GCSNetworkPlanner/docs/implementation/BUILD.md)
- [Architecture](/home/boots/work/phd/GCSNetworkPlanner/docs/implementation/ARCHITECTURE.md)
- [Platform Capabilities](/home/boots/work/phd/GCSNetworkPlanner/docs/implementation/PLATFORM_CAPABILITIES.md)
- [Project Layout](/home/boots/work/phd/GCSNetworkPlanner/docs/implementation/PROJECT_LAYOUT.md)
- [Configuration](/home/boots/work/phd/GCSNetworkPlanner/docs/implementation/CONFIGURATION.md)
- [Video Simulation](/home/boots/work/phd/GCSNetworkPlanner/docs/implementation/VIDEO_SIMULATION.md)
- [Troubleshooting](/home/boots/work/phd/GCSNetworkPlanner/docs/implementation/TROUBLESHOOTING.md)
- [Contributing](/home/boots/work/phd/GCSNetworkPlanner/docs/implementation/CONTRIBUTING.md)
- [ns-3 Simulation Workspace](/home/boots/work/phd/GCSNetworkPlanner/sim/ns3/README.md)
- [Security Policy](/home/boots/work/phd/GCSNetworkPlanner/SECURITY.md)
- [Support](/home/boots/work/phd/GCSNetworkPlanner/SUPPORT.md)
- [Code of Conduct](/home/boots/work/phd/GCSNetworkPlanner/CODE_OF_CONDUCT.md)

## Screenshots

Project screenshots can be published under `docs/images/`.

## Known Limitations

- The legacy DJI SDK and MAVLink vendor trees remain in-tree for compatibility and still carry upstream technical debt.
- Automated testing is limited; smoke testing is currently the main verification path.
- Some control and UI flows are still shaped by earlier prototype-era architecture.
- Forced termination currently exposes an existing shutdown cleanup bug (`free(): invalid pointer`) that still needs dedicated debugging.
- Some QML files still emit legacy lint warnings even though the application runs.
- Cellular simulation experiments currently cover LTE and NR with the downloaded `ns-allinone-3.47` bundle; GSM/UMTS are not present in that local release.
- The live ns-3 visualization bridge uses projected GPS coordinates and localhost UDP snapshots.
- The video path is a metric-shaped impairment model driven by ns-3 metrics, not a full RTP/RTSP transport stack inside ns-3.
- Some selected UAVs may briefly show partial vehicle state until the mirrored per-UAV MAVLink cache has received its first packets.

## License

This repository is distributed under the terms of the [LICENSE](/home/boots/work/phd/GCSNetworkPlanner/LICENSE) file in the project root.

## Contributing

Contributions are welcome, but contributors should read the project documentation first:

- [docs/implementation/CONTRIBUTING.md](/home/boots/work/phd/GCSNetworkPlanner/docs/implementation/CONTRIBUTING.md)
- [CODE_OF_CONDUCT.md](/home/boots/work/phd/GCSNetworkPlanner/CODE_OF_CONDUCT.md)
- [SECURITY.md](/home/boots/work/phd/GCSNetworkPlanner/SECURITY.md)

## Support

If you are trying to build, configure, or debug the project, start here:

- [SUPPORT.md](/home/boots/work/phd/GCSNetworkPlanner/SUPPORT.md)
- [docs/implementation/TROUBLESHOOTING.md](/home/boots/work/phd/GCSNetworkPlanner/docs/implementation/TROUBLESHOOTING.md)
