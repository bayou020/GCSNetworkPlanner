# Project Layout

## Top-Level Directories

### `src/`

First-party C++ code grouped by operational domain.

- `app/`
  application startup, QML engine setup, context-property registration
- `core/`
  shared utilities such as logging and file writing
- `dji/`
  DJI-specific integration and onboard SDK wiring
- `mavlink/`
  MAVLink decoding, joystick integration, serial readers, mission/control glue
- `network/`
  UDP and modem-related communication paths
- `plot/`
  plotting bridge code and QCustomPlot integration wrappers
- `ui/`
  Qt/QML-facing helpers such as image providers and joystick parameter adapters
- `weather/`
  OpenWeather-based marker and tile configuration logic

### `qml/`

Qt Quick UI organized by role.

- `screens/`
  top-level view composition
- `maps/`
  map view logic and interaction handlers
- `components/`
  reusable UI widgets and support views

### `assets/`

Packaged static assets used by the UI. Runtime access is provided through `resources/assets.qrc`.

### `resources/`

Qt resource manifests and packaged configuration files.

- `qml.qrc`
  bundles the QML tree and preserves stable resource aliases
- `assets.qrc`
  bundles icon assets and preserves stable `qrc:/ico/...` paths
- `qtquickcontrols2.conf`
  Qt Quick Controls runtime configuration

### `third_party/`

Vendored code preserved in-tree.

- `dji_sdk/`
- `mavlink/`
- `QJoysticks/`
- `qcustomplot/`
- `qfi/`

### `legacy/`

Archived code from older Qt 5 and widget-era iterations. These files are retained for reference only and are not part of the active build.

### `scripts/`

Automation for dependency installation, configuration, build, and launch.

### `docs/`

Project-facing documentation for users and contributors.

## Design Principles

- keep first-party and third-party code clearly separated
- keep root-level clutter minimal
- preserve stable runtime QRC paths even when physical files move
- prefer explicit environment-driven configuration over local hardcoding

