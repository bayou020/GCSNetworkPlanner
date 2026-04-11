#!/usr/bin/env bash
set -euo pipefail

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
qt_root="${QT_ROOT:-$HOME/Qt/6.11.0/gcc_64}"
build_dir="${BUILD_DIR:-$project_root/build}"
maplibre_install_prefix="${MAPLIBRE_INSTALL_PREFIX:-$project_root/.deps/maplibre-native-qt/install}"
env_file="${ENV_FILE:-$project_root/env}"
app_binary="${APP_BINARY:-$build_dir/bin/NetworkPlannerGCS}"

prepend_path() {
    local dir="$1"
    local var_name="$2"
    if [[ ! -d "$dir" ]]; then
        return
    fi
    local current="${!var_name:-}"
    if [[ -n "$current" ]]; then
        export "$var_name=$dir:$current"
    else
        export "$var_name=$dir"
    fi
}

if [[ -f "$env_file" ]]; then
    echo "==> Loading environment from $env_file"
    # shellcheck disable=SC1090
    source "$env_file"
fi

if [[ ! -x "$app_binary" ]]; then
    echo "Application binary not found at $app_binary" >&2
    echo "Run ./scripts/build.sh first or override APP_BINARY." >&2
    exit 1
fi

export QSG_RHI_BACKEND="${QSG_RHI_BACKEND:-opengl}"
prepend_path "$maplibre_install_prefix/lib" LD_LIBRARY_PATH
prepend_path "$qt_root/lib" LD_LIBRARY_PATH
prepend_path "$maplibre_install_prefix/plugins" QT_PLUGIN_PATH
prepend_path "$qt_root/plugins" QT_PLUGIN_PATH
prepend_path "$maplibre_install_prefix/qml" QML_IMPORT_PATH
prepend_path "$qt_root/qml" QML_IMPORT_PATH

echo "==> Launching $app_binary"
exec "$app_binary" "$@"
