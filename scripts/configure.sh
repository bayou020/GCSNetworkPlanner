#!/usr/bin/env bash
set -euo pipefail

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
qt_root="${QT_ROOT:-$HOME/Qt/6.11.0/gcc_64}"
build_dir="${BUILD_DIR:-$project_root/build}"
maplibre_install_prefix="${MAPLIBRE_INSTALL_PREFIX:-$project_root/.deps/maplibre-native-qt/install}"
build_type="${CMAKE_BUILD_TYPE:-RelWithDebInfo}"

if [[ ! -x "$qt_root/bin/qt-cmake" ]]; then
    echo "Qt was not found at QT_ROOT=$qt_root" >&2
    echo "Install Qt 6.11 and export QT_ROOT to the gcc_64 directory." >&2
    exit 1
fi

prefix_path="$qt_root"
if [[ -d "$maplibre_install_prefix/lib/cmake/QMapLibre" ]]; then
    prefix_path="$maplibre_install_prefix;$prefix_path"
else
    echo "MapLibre Native Qt was not found at $maplibre_install_prefix." >&2
    echo "Run ./scripts/setup_maplibre_qt6.sh or override MAPLIBRE_INSTALL_PREFIX before configuring." >&2
    exit 1
fi

echo "==> Configuring project in $build_dir"
"$qt_root/bin/qt-cmake" \
    -S "$project_root" \
    -B "$build_dir" \
    -G Ninja \
    -DCMAKE_BUILD_TYPE="$build_type" \
    -DMAPLIBRE_INSTALL_PREFIX="$maplibre_install_prefix" \
    -DCMAKE_PREFIX_PATH="$prefix_path${CMAKE_PREFIX_PATH:+;$CMAKE_PREFIX_PATH}" \
    "$@"

echo "==> Configure step completed"
