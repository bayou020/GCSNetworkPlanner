#!/usr/bin/env bash
set -euo pipefail

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${BUILD_DIR:-$project_root/build}"
build_jobs="${BUILD_JOBS:-$(nproc)}"

if [[ ! -f "$build_dir/build.ninja" && ! -f "$build_dir/Makefile" ]]; then
    echo "==> Build directory is not configured yet; running configure.sh"
    "$project_root/scripts/configure.sh"
fi

echo "==> Building project"
cmake --build "$build_dir" --parallel "$build_jobs" "$@"

