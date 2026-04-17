#!/usr/bin/env bash
set -euo pipefail

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${BUILD_DIR:-$project_root/build}"
maplibre_install_prefix="${MAPLIBRE_INSTALL_PREFIX:-$project_root/.deps/maplibre-native-qt/install}"
env_file="${ENV_FILE:-$project_root/env}"
app_binary="${APP_BINARY:-$build_dir/bin/NetworkPlannerGCS}"

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

query_qt_install_path() {
    local key="$1"
    local qtpaths_bin=""

    for qtpaths_bin in \
        "$qt_root/bin/qtpaths6" \
        "$qt_root/bin/qtpaths"; do
        if [[ -x "$qtpaths_bin" ]]; then
            "$qtpaths_bin" --query "$key" 2>/dev/null
            return 0
        fi
    done

    return 1
}

qt_root="$(detect_qt_root || true)"
qt_plugin_dir="${QT_PLUGIN_DIR:-$qt_root/plugins}"
qt_qml_dir="${QT_QML_DIR:-$qt_root/qml}"
qt_lib_dir="${QT_LIB_DIR:-$qt_root/lib}"

if [[ -n "$qt_root" ]]; then
    qt_plugin_dir="${QT_PLUGIN_DIR:-$(query_qt_install_path QT_INSTALL_PLUGINS || printf '%s' "$qt_plugin_dir")}"
    qt_qml_dir="${QT_QML_DIR:-$(query_qt_install_path QT_INSTALL_QML || printf '%s' "$qt_qml_dir")}"
    qt_lib_dir="${QT_LIB_DIR:-$(query_qt_install_path QT_INSTALL_LIBS || printf '%s' "$qt_lib_dir")}"
fi

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

if [[ "$(uname -s)" == "Darwin" ]]; then
    if [[ -z "${QSG_RHI_BACKEND:-}" || "${QSG_RHI_BACKEND}" == "opengl" ]]; then
        export QSG_RHI_BACKEND="metal"
    fi
else
    export QSG_RHI_BACKEND="${QSG_RHI_BACKEND:-opengl}"
fi

export NP_GCS_NS3_UI_UPDATE_MS="${NP_GCS_NS3_UI_UPDATE_MS:-75}"

library_path_var="LD_LIBRARY_PATH"
if [[ "$(uname -s)" == "Darwin" ]]; then
    library_path_var="DYLD_LIBRARY_PATH"
fi

prepend_path "$maplibre_install_prefix/lib" "$library_path_var"
prepend_path "$qt_lib_dir" "$library_path_var"
prepend_path "$maplibre_install_prefix/plugins" QT_PLUGIN_PATH
prepend_path "$qt_plugin_dir" QT_PLUGIN_PATH
prepend_path "$maplibre_install_prefix/qml" QML_IMPORT_PATH
prepend_path "$qt_qml_dir" QML_IMPORT_PATH

echo "==> Launching $app_binary"
exec "$app_binary" "$@"
