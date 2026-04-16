#!/usr/bin/env bash
set -euo pipefail

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
default_local_maplibre_src_dir="$(cd "$project_root/.." && pwd)/maplibre-native-qt"
if [[ -d "$default_local_maplibre_src_dir/.git" ]]; then
    default_maplibre_src_dir="$default_local_maplibre_src_dir"
else
    default_maplibre_src_dir="$project_root/.deps/maplibre-native-qt/src"
fi
maplibre_src_dir="${MAPLIBRE_SRC_DIR:-$default_maplibre_src_dir}"
maplibre_build_dir="${MAPLIBRE_BUILD_DIR:-$project_root/.deps/maplibre-native-qt/build}"
maplibre_install_prefix="${MAPLIBRE_INSTALL_PREFIX:-$project_root/.deps/maplibre-native-qt/install}"
maplibre_repo_url="${MAPLIBRE_REPO_URL:-https://github.com/maplibre/maplibre-native-qt.git}"
build_type="${CMAKE_BUILD_TYPE:-Release}"

detect_qt_root() {
    if [[ -n "${QT_ROOT:-}" ]]; then
        printf '%s\n' "$QT_ROOT"
        return 0
    fi

    local candidates=(
        "$HOME/Qt/6.11.0/macos"
        "$HOME/Qt/6.11.0/gcc_64"
    )

    if command -v brew >/dev/null 2>&1; then
        local brew_qt_prefix
        for formula in qt qt@6; do
            if brew_qt_prefix="$(brew --prefix "$formula" 2>/dev/null)"; then
                candidates+=("$brew_qt_prefix")
            fi
        done
    fi

    local candidate
    for candidate in "${candidates[@]}"; do
        if [[ -x "$candidate/bin/qt-cmake" ]]; then
            printf '%s\n' "$candidate"
            return 0
        fi
    done

    return 1
}

detect_job_count() {
    if command -v nproc >/dev/null 2>&1; then
        nproc
        return
    fi

    if command -v sysctl >/dev/null 2>&1; then
        sysctl -n hw.ncpu
        return
    fi

    printf '4\n'
}

qt_root="$(detect_qt_root || true)"
build_jobs="${BUILD_JOBS:-$(detect_job_count)}"

if [[ ! -x "$qt_root/bin/qt-cmake" ]]; then
    echo "Qt 6.11 was not found at QT_ROOT=$qt_root" >&2
    echo "Install Qt 6.11 and export QT_ROOT to the Qt host directory before running this script." >&2
    exit 1
fi

mkdir -p "$(dirname "$maplibre_src_dir")" "$maplibre_build_dir" "$maplibre_install_prefix"

if [[ ! -d "$maplibre_src_dir/.git" ]]; then
    echo "==> Cloning MapLibre Native Qt into $maplibre_src_dir"
    git clone --depth 1 --recurse-submodules --shallow-submodules -j8 \
        "$maplibre_repo_url" "$maplibre_src_dir"
else
    echo "==> Reusing existing MapLibre Native Qt checkout at $maplibre_src_dir"
fi

echo "==> Syncing MapLibre Native Qt submodules"
git -C "$maplibre_src_dir" submodule update --init --recursive --depth 1 --jobs 8

cmake_args=(
    -S "$maplibre_src_dir"
    -B "$maplibre_build_dir"
    -G Ninja
    -DCMAKE_BUILD_TYPE="$build_type"
    -DCMAKE_TOOLCHAIN_FILE="$qt_root/lib/cmake/Qt6/qt.toolchain.cmake"
    -DCMAKE_INSTALL_PREFIX="$maplibre_install_prefix"
)

if [[ "$(uname -s)" == "Darwin" ]]; then
    cmake_args+=(
        -DMLN_WITH_METAL=ON
        -DCMAKE_OSX_ARCHITECTURES="${CMAKE_OSX_ARCHITECTURES:-arm64}"
        -DCMAKE_OSX_DEPLOYMENT_TARGET="${CMAKE_OSX_DEPLOYMENT_TARGET:-13.0}"
    )
else
    cmake_args+=(-DMLN_WITH_OPENGL=ON)
fi

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
