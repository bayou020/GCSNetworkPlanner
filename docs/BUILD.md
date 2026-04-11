# Build Guide

## Supported Platform

The maintained path is Ubuntu/Linux with:

- Qt 6.11 from the official Qt installer
- MapLibre Native Qt built locally against the same Qt toolchain
- SDL2 from the system package manager

## 1. Install System Packages

```bash
./scripts/setup_ubuntu_deps.sh
```

This installs the compiler toolchain, Ninja/CMake tooling, SDL2, and the native libraries needed to build MapLibre Native Qt.

## 2. Install Qt 6.11

Install Qt 6.11 with the official Qt installer and export `QT_ROOT` to the `gcc_64` directory:

```bash
export QT_ROOT="$HOME/Qt/6.11.0/gcc_64"
```

The project does not rely on distro-provided Qt packages for the primary build path.

## 3. Build MapLibre Native Qt

```bash
./scripts/setup_maplibre_qt6.sh
```

By default this installs into:

```text
.deps/maplibre-native-qt/install
```

The project configuration script uses that prefix automatically.

## 4. Configure the Project

```bash
./scripts/configure.sh
```

Important overrides:

- `QT_ROOT`
- `MAPLIBRE_INSTALL_PREFIX`
- `BUILD_DIR`
- `CMAKE_BUILD_TYPE`

## 5. Build

```bash
./scripts/build.sh
```

The executable is written to:

```text
build/bin/NetworkPlannerGCS
```

## 6. Run

```bash
cp env.example env
# edit env with your Mapbox and OpenWeather values
./scripts/run.sh
```

## Manual Configure Example

If you prefer a direct CMake invocation:

```bash
"$QT_ROOT/bin/qt-cmake" \
  -S . \
  -B build \
  -G Ninja \
  -DMAPLIBRE_INSTALL_PREFIX="$PWD/.deps/maplibre-native-qt/install" \
  -DCMAKE_PREFIX_PATH="$PWD/.deps/maplibre-native-qt/install;$QT_ROOT"

cmake --build build --parallel
```

## Smoke Test

The basic verification pass is:

1. configure successfully
2. build successfully
3. launch with `./scripts/run.sh`
4. confirm the QML UI loads and the MapLibre plugin is available

## ns-3 Cellular Research Workspace

The repository also contains an external `ns-3` experiment workspace under:

```text
sim/ns3/
```

This is intentionally separate from the Qt application build. It uses the local archive:

```text
~/Downloads/ns-allinone-3.47.tar.bz2
```

Quick start:

```bash
./sim/ns3/scripts/run_uav_lte.sh
./sim/ns3/scripts/run_uav_nr.sh
```

Those scripts extract the archive into `.deps/`, stage the repo scenarios into the ns-3 `scratch/` tree, build a focused ns-3 dependency set, and run the experiment.

For live QML visualization against the desktop GCS:

```bash
./scripts/run.sh
./sim/ns3/scripts/run_live_uav_lte.sh
```

or:

```bash
./scripts/run.sh
./sim/ns3/scripts/run_live_uav_nr.sh
```
