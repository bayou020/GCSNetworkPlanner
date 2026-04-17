#!/usr/bin/env bash
set -euo pipefail

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${BUILD_DIR:-$project_root/build}"

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

build_jobs="${BUILD_JOBS:-$(detect_job_count)}"

if [[ ! -f "$build_dir/build.ninja" && ! -f "$build_dir/Makefile" ]]; then
    echo "==> Build directory is not configured yet; running configure.sh"
    "$project_root/scripts/configure.sh"
fi

echo "==> Building project"
cmake --build "$build_dir" --parallel "$build_jobs" "$@"
