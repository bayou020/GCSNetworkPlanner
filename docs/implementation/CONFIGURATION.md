# Configuration

## Environment Files

Use `env.example` as the tracked template and create a local `env` file at the repository root:

```bash
cp env.example env
```

`./scripts/run.sh` loads `env` automatically when present.

## Required Variables

### Mapbox / MapLibre Base Map

- `MAPBOX_ACCESS_TOKEN`
  - public Mapbox token used by the MapLibre provider
- `MAPBOX_STYLE_URL`
  - style URL such as `mapbox://styles/<account>/<style-id>`

The application expects both values when using the Mapbox-backed base map.

### OpenWeather

- `OPENWEATHERMAP_API_KEY`
  - used for click-driven weather requests
  - also used to build the raster weather tile URL
- `OPENWEATHERMAP_TILE_LAYER`
  - raster overlay layer selector
  - supported values include:
    - `WND`
    - `TA2`
    - `APM`
    - `CL`
    - `PA0`

The project uses the legacy tile endpoint because it is compatible with the current OpenWeather account flow:

```text
https://tile.openweathermap.org/map/<layer>/{z}/{x}/{y}.png?appid=<key>
```

### NetworkPlannerRpi Simulation / Replay

The GCS UDP bridge still defaults to the original VPN addresses for real hardware:

- pilot: `10.8.0.62:14551 <-> 10.8.0.66:14552`
- modem: `10.8.0.62:14581 <-> 10.8.0.66:14582`
- gimbal: `10.8.0.62:14591 <-> 10.8.0.66:14592`

For local interoperability with `NetworkPlannerRpi`, the GCS now understands the same
channel environment variables used by the RPi repo and automatically inverts them for the
local bind/send sockets:

- `NPRPI_PILOT_HOST`
- `NPRPI_PILOT_BIND_PORT`
- `NPRPI_PILOT_REMOTE_PORT`
- `NPRPI_MODEM_HOST`
- `NPRPI_MODEM_BIND_PORT`
- `NPRPI_MODEM_REMOTE_PORT`
- `NPRPI_GIMBAL_HOST`
- `NPRPI_GIMBAL_BIND_PORT`
- `NPRPI_GIMBAL_REMOTE_PORT`

If you need to override the GCS transport directly, use:

- `NPGCS_PILOT_BIND_HOST`
- `NPGCS_PILOT_BIND_PORT`
- `NPGCS_PILOT_REMOTE_HOST`
- `NPGCS_PILOT_REMOTE_PORT`
- `NPGCS_MODEM_BIND_HOST`
- `NPGCS_MODEM_BIND_PORT`
- `NPGCS_MODEM_REMOTE_HOST`
- `NPGCS_MODEM_REMOTE_PORT`
- `NPGCS_GIMBAL_BIND_HOST`
- `NPGCS_GIMBAL_BIND_PORT`
- `NPGCS_GIMBAL_REMOTE_HOST`
- `NPGCS_GIMBAL_REMOTE_PORT`

Recommended local setup when pairing this repo with `NetworkPlannerRpi` replay or simulation:

```bash
export NPRPI_PILOT_HOST='127.0.0.1'
export NPRPI_MODEM_HOST='127.0.0.1'
export NPRPI_GIMBAL_HOST='127.0.0.1'
```

Important: `NetworkPlannerRpi/scripts/run_sim.sh` only emits a synthetic MAVLink heartbeat.
If you need battery, GPS, and other live vehicle fields in the GCS, use:

- `NetworkPlannerRpi/scripts/run_replay.sh` for deterministic telemetry replay
- `NetworkPlannerRpi/scripts/run_sitl.sh` for live SITL-backed MAVLink
- `NetworkPlannerRpi/scripts/run_ns3_live.sh` when pairing the RPi simulator with the live ns-3 fleet feed

For the integrated ns-3 + RPi + GCS workflow from this repository, use:

- `sim/ns3/scripts/run_live_uav_lte_with_rpi.sh`
- `sim/ns3/scripts/run_live_uav_nr_with_rpi.sh`

The integrated wrappers split the live snapshot traffic into two localhost ports:

- `NS3_SIM_PORT`
  - default `45454`
  - consumed by the GCS `simulationFeed` for multi-UAV map rendering
- `NS3_SIM_RPI_PORT`
  - default `45455`
  - consumed by `NetworkPlannerRpi/scripts/run_ns3_live.sh`

This split matters. If both the GCS and the RPi bridge bind the same unicast UDP port, only one
consumer may receive the live ns-3 snapshots and the map can collapse back to the legacy single-UAV
marker.

### Simulated Video Transport

The live stack can also push a simulated video feed from `NetworkPlannerRpi` to the GCS.

Shared endpoint variables:

- `NPVIDEO_HOST`
  - default `127.0.0.1`
- `NPVIDEO_PORT`
  - default `5600`

RPi-side video controls:

- `NPRPI_ENABLE_VIDEO_SIMULATION`
- `NPRPI_VIDEO_HOST`
- `NPRPI_VIDEO_PORT`
- `NPRPI_VIDEO_WIDTH`
- `NPRPI_VIDEO_HEIGHT`
- `NPRPI_VIDEO_FPS`
- `NPRPI_VIDEO_JPEG_QUALITY`
- `NPRPI_VIDEO_PATTERN`
- `NPRPI_VIDEO_MAX_DELAY_MS`

The selected UAV in the GCS drives which simulated video stream is shown in the right-side video panel.

See:

- [docs/implementation/VIDEO_SIMULATION.md](/home/boots/work/phd/GCSNetworkPlanner/docs/implementation/VIDEO_SIMULATION.md)

### Qt Runtime

- `QSG_RHI_BACKEND`
  - default recommended value: `opengl`

## Build-Time Overrides

These are typically exported before running the scripts:

- `QT_ROOT`
  - path to the official Qt 6.11 `gcc_64` directory
- `MAPLIBRE_INSTALL_PREFIX`
  - install prefix for the local MapLibre Native Qt build
- `BUILD_DIR`
  - alternate project build directory

## MapLibre Integration Notes

- The repository does not use Qt 5-era Mapbox GL bindings.
- The maintained map path is MapLibre Native Qt plus Qt Location / QML integration.
- `QMapLibre` must be discoverable by CMake under `MAPLIBRE_INSTALL_PREFIX/lib/cmake/QMapLibre`.
- `scripts/run.sh` sets `QML_IMPORT_PATH`, `QT_PLUGIN_PATH`, and `LD_LIBRARY_PATH` for the local MapLibre install and Qt runtime.
