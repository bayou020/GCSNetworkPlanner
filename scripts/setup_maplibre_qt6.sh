#!/usr/bin/env bash
set -euo pipefail

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
qt_root="${QT_ROOT:-$HOME/Qt/6.11.0/gcc_64}"
maplibre_src_dir="${MAPLIBRE_SRC_DIR:-$project_root/.deps/maplibre-native-qt/src}"
maplibre_build_dir="${MAPLIBRE_BUILD_DIR:-$project_root/.deps/maplibre-native-qt/build}"
maplibre_install_prefix="${MAPLIBRE_INSTALL_PREFIX:-$project_root/.deps/maplibre-native-qt/install}"
maplibre_repo_url="${MAPLIBRE_REPO_URL:-https://github.com/maplibre/maplibre-native-qt.git}"
build_type="${CMAKE_BUILD_TYPE:-Release}"
build_jobs="${BUILD_JOBS:-$(nproc)}"

if [[ ! -x "$qt_root/bin/qt-cmake" ]]; then
    echo "Qt 6.11 was not found at QT_ROOT=$qt_root" >&2
    echo "Install Qt 6.11 and export QT_ROOT to the gcc_64 directory before running this script." >&2
    exit 1
fi

mkdir -p "$(dirname "$maplibre_src_dir")" "$maplibre_build_dir" "$maplibre_install_prefix"

if [[ ! -d "$maplibre_src_dir/.git" ]]; then
    echo "==> Cloning MapLibre Native Qt into $maplibre_src_dir"
    git clone --recurse-submodules -j8 "$maplibre_repo_url" "$maplibre_src_dir"
else
    echo "==> Reusing existing MapLibre Native Qt checkout at $maplibre_src_dir"
fi

echo "==> Syncing MapLibre Native Qt submodules"
git -C "$maplibre_src_dir" submodule update --init --recursive

cmake_args=(
    -S "$maplibre_src_dir"
    -B "$maplibre_build_dir"
    -G Ninja
    -DMLN_WITH_OPENGL=ON
    -DCMAKE_BUILD_TYPE="$build_type"
    -DCMAKE_TOOLCHAIN_FILE="$qt_root/lib/cmake/Qt6/qt.toolchain.cmake"
    -DCMAKE_INSTALL_PREFIX="$maplibre_install_prefix"
)

if command -v ccache >/dev/null 2>&1; then
    cmake_args+=(
        -DCMAKE_C_COMPILER_LAUNCHER=ccache
        -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
    )
fi

echo "==> Configuring MapLibre Native Qt"
cmake "${cmake_args[@]}"

echo "==> Building MapLibre Native Qt"
cmake --build "$maplibre_build_dir" --parallel "$build_jobs"

echo "==> Installing MapLibre Native Qt to $maplibre_install_prefix"
cmake --install "$maplibre_build_dir"

cat <<EOF

MapLibre Native Qt is installed.

Useful environment exports:
  export MAPLIBRE_INSTALL_PREFIX="$maplibre_install_prefix"
  export QMapLibre_DIR="$maplibre_install_prefix/lib/cmake/QMapLibre"

Next steps:
  ./scripts/configure.sh
  ./scripts/build.sh

EOF
