# Architecture

## Overview

NetworkPlannerGCS is a Qt desktop application composed of a QML front end and a set of C++ services exposed into the QML engine. The application is still shaped by some earlier prototype-era constraints, but the repository is now organized around operational domains rather than ad hoc root-level files.

## Runtime Layers

### UI Layer

- `qml/screens/` contains top-level application views such as `UavMapForm.qml`.
- `qml/maps/` contains map-specific views and interaction logic.
- `qml/components/` contains reusable UI elements such as joystick, plot, and network panels.

### Application / Service Layer

- `src/app/` contains startup wiring and context-property registration.
- `src/ui/` provides Qt/QML bridge helpers such as image providers and joystick parameter adapters.
- `src/weather/` provides click-driven weather lookups and raster weather tile template generation.
- `src/plot/` bridges QCustomPlot into the QML UI.

### Telemetry / Control Layer

- `src/mavlink/` handles MAVLink message parsing, joystick interactions, serial input, and mission/control commands.
- `src/network/` handles UDP and modem-related communication paths.
- `src/dji/` keeps DJI-specific integration isolated from the rest of the telemetry stack.

### Vendor Layer

- `third_party/dji_sdk/` contains the vendored DJI SDK.
- `third_party/mavlink/` contains vendored MAVLink C libraries.
- `third_party/QJoysticks/` contains the joystick abstraction layer.
- `third_party/qcustomplot/` and `third_party/qfi/` contain plotting and flight-instrument vendor code.

### Legacy Archive

- `legacy/` stores obsolete Qt 5 / widget-era files that are retained as historical reference, not part of the modern build.

## Map Stack

The map path is:

1. `src/app/main.cpp` exposes configuration and service objects to QML.
2. `qml/maps/UavMap.qml` instantiates a `QtLocation` `Map` using the MapLibre provider.
3. MapLibre consumes a Mapbox style URL and access token supplied through environment variables.
4. Weather overlays are layered in two ways:
   - click-based marker data from `WeatherService`
   - raster weather tiles added into the active MapLibre style

## Telemetry Flow

High-level telemetry/control flow:

1. serial / UDP / joystick inputs enter through `src/mavlink/` and `src/network/`
2. decoded values are forwarded into plotting, UI, and command handlers
3. QML emits mission and flight-control signals back to the C++ layer
4. auxiliary UI systems such as plots and flight instruments update via context properties and signals

## Build / Packaging Model

- CMake is the primary build system.
- QML and icon assets are packaged through `resources/qml.qrc` and `resources/assets.qrc`.
- QRC aliases are used to keep stable runtime URLs while allowing physical files to live in cleaner folders.
