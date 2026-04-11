# NetworkPlannerGCS

NetworkPlannerGCS is an open-source Qt 6.11 ground control station for UAV telemetry, map-centric mission planning, joystick-driven control workflows, and network-aware field operations. The application combines a QML front end with C++ services for MAVLink, DJI integration, MapLibre/Mapbox rendering, plotting, and OpenWeather overlays.

This repository began as a PhD-era research project and is now being maintained as a cleaned-up public codebase with a reproducible build flow, explicit environment-based configuration, and a documented project layout.

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
- MAVLink telemetry decoding and command dispatch
- DJI Onboard SDK integration retained from the original research codebase
- SDL2-backed joystick support through QJoysticks
- QCustomPlot-based charting and flight-instrument overlays
- C++/QML bridge layer for telemetry, controls, plotting, and map interaction

## Repository Highlights

- Clean domain-oriented source layout under `src/`
- QML separated into screens, map views, and reusable components
- Vendored dependencies isolated under `third_party/`
- Legacy Qt 5/widget-era code archived under `legacy/`
- Setup/configure/build/run scripts under `scripts/`
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

## Configuration

Runtime configuration is driven by environment variables rather than hardcoded project-local secrets. The tracked template is [env.example](/home/boots/work/phd/GCSNetworkPlanner/env.example); your machine-local copy should live in `env`.

Core variables:

- `MAPBOX_ACCESS_TOKEN`
- `MAPBOX_STYLE_URL`
- `OPENWEATHERMAP_API_KEY`
- `OPENWEATHERMAP_TILE_LAYER`
- `QSG_RHI_BACKEND`

Full reference:

- [docs/CONFIGURATION.md](/home/boots/work/phd/GCSNetworkPlanner/docs/CONFIGURATION.md)
- [docs/TROUBLESHOOTING.md](/home/boots/work/phd/GCSNetworkPlanner/docs/TROUBLESHOOTING.md)

## Project Layout

```text
src/          First-party C++ application and service code
qml/          QML screens, map views, and reusable UI components
assets/       Icons and packaged runtime visuals
resources/    QRC manifests and Qt resource configuration
third_party/  Vendored dependencies preserved in-tree
legacy/       Archived code excluded from the active build
scripts/      Bootstrap, configure, build, and run automation
docs/         Build, architecture, configuration, troubleshooting, and contribution guides
```

Detailed layout notes:

- [docs/PROJECT_LAYOUT.md](/home/boots/work/phd/GCSNetworkPlanner/docs/PROJECT_LAYOUT.md)
- [docs/ARCHITECTURE.md](/home/boots/work/phd/GCSNetworkPlanner/docs/ARCHITECTURE.md)

## Documentation Index

- [Build Guide](/home/boots/work/phd/GCSNetworkPlanner/docs/BUILD.md)
- [Architecture](/home/boots/work/phd/GCSNetworkPlanner/docs/ARCHITECTURE.md)
- [Project Layout](/home/boots/work/phd/GCSNetworkPlanner/docs/PROJECT_LAYOUT.md)
- [Configuration](/home/boots/work/phd/GCSNetworkPlanner/docs/CONFIGURATION.md)
- [Troubleshooting](/home/boots/work/phd/GCSNetworkPlanner/docs/TROUBLESHOOTING.md)
- [Contributing](/home/boots/work/phd/GCSNetworkPlanner/docs/CONTRIBUTING.md)
- [Security Policy](/home/boots/work/phd/GCSNetworkPlanner/SECURITY.md)
- [Support](/home/boots/work/phd/GCSNetworkPlanner/SUPPORT.md)
- [Code of Conduct](/home/boots/work/phd/GCSNetworkPlanner/CODE_OF_CONDUCT.md)

## Screenshots

Project screenshots can be published under `docs/images/`.

## Known Limitations

- The legacy DJI SDK and MAVLink vendor trees remain in-tree for compatibility and still carry upstream technical debt.
- Automated testing is limited; smoke testing is currently the main verification path.
- Some control and UI flows are still shaped by the original research prototype architecture.
- Forced termination currently exposes an existing shutdown cleanup bug (`free(): invalid pointer`) that still needs dedicated debugging.
- Some QML files still emit legacy lint warnings even though the application runs.

## License

This repository is distributed under the terms of the [LICENSE](/home/boots/work/phd/GCSNetworkPlanner/LICENSE) file in the project root.

## Contributing

Contributions are welcome, but contributors should read the project documentation first:

- [docs/CONTRIBUTING.md](/home/boots/work/phd/GCSNetworkPlanner/docs/CONTRIBUTING.md)
- [CODE_OF_CONDUCT.md](/home/boots/work/phd/GCSNetworkPlanner/CODE_OF_CONDUCT.md)
- [SECURITY.md](/home/boots/work/phd/GCSNetworkPlanner/SECURITY.md)

## Support

If you are trying to build, configure, or debug the project, start here:

- [SUPPORT.md](/home/boots/work/phd/GCSNetworkPlanner/SUPPORT.md)
- [docs/TROUBLESHOOTING.md](/home/boots/work/phd/GCSNetworkPlanner/docs/TROUBLESHOOTING.md)
