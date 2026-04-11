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

