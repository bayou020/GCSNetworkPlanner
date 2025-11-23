# GCSNetworkPlanner

Ground Control Station desktop app built with Qt 5. It combines MAVLink telemetry/control, DJI Onboard SDK integration, joystick input, live plotting, and cellular modem analytics in a single UI.

## Features
- Multi‑protocol: MAVLink (PX4/ArduPilot) and DJI OSDK (serial link).
- Joystick input via SDL2 with virtual joystick support.
- Live QML UI with flight instruments, map, plots, and network quality banner.
- Cellular modem decoding (LTE/WCDMA/GSM) with signal range indicators.
- UDP bridges for pilot, modem, and gimbal traffic; basic logging and file writing utilities.

## Project layout
- `main.cpp` – Qt app entry; wires QML, MAVLink, DJI, joystick, modem, plots.
- `qml/*.qml` & `*.qml` – QML UI files.
- `src_qfi/` – flight instrument widgets.
- `mavlink/` – MAVLink message handling and c_library_v1/v2 headers.
- `dji_sdk/` – DJI Onboard SDK sources (3.1.x era) used directly in the build.
- `QJoysticks/` – joystick wrapper library (uses SDL2).

## Prerequisites (macOS example)
- Qt 5.15.x with modules: Core, Gui, Widgets, Quick, QuickWidgets, QML, Network, SerialPort, SVG, PrintSupport, Multimedia.
- SDL2 (Homebrew: `brew install sdl2`).
- A compiler toolchain (Xcode CLT) and `qmake`.

## Build
```bash
cd /Users/macbook/work/uav/GCSNetworkPlanner   # adjust to your path
/opt/homebrew/opt/qt@5/bin/qmake NetworkPlanner.pro
make -j$(sysctl -n hw.ncpu)
```
The build outputs `bin/NetworkPlannerGCS.app`.

## Run
```bash
cd bin
open NetworkPlannerGCS.app
# or
./NetworkPlannerGCS.app/Contents/MacOS/NetworkPlannerGCS
```
Ensure any required hardware is connected (serial link to flight controller/DJI, joystick, modem).

## Notes
- Qt warns about macOS SDK > 14 when using Qt 5.15; it builds but is unsupported upstream.
- MAVLink v2 headers are used for decoding/encoding; v1 headers remain for some helper classes.
- DJI activation/keys and actual vehicle/RC connections are required to exercise DJI features.
