#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
NS3_BASE_DIR="${NS3_BASE_DIR:-$ROOT_DIR/.deps}"
NS3_ROOT="${NS3_ROOT:-$NS3_BASE_DIR/ns-allinone-3.47/ns-3.47}"
BUILD_DIR="${NS3_BUILD_DIR:-$NS3_ROOT/cmake-ran-cache}"
OUTPUT_DIR="${NS3_OUTPUT_DIR:-$NS3_ROOT/build-ran}"
ENABLED_MODULES="${NS3_ENABLED_MODULES:-antenna;applications;buildings;core;flow-monitor;internet;lte;mobility;network;nr;point-to-point;propagation;spectrum;stats}"

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

NPROC="${NPROC:-$(detect_job_count)}"

"$ROOT_DIR/sim/ns3/scripts/setup_ns3.sh"

cd "$NS3_ROOT"

ccache_usable() {
  if ! command -v ccache >/dev/null 2>&1; then
    return 1
  fi

  if ! command -v python3 >/dev/null 2>&1; then
    return 1
  fi

  python3 - <<'PY'
import shutil
import subprocess
import sys

ccache = shutil.which("ccache")
if not ccache:
    sys.exit(1)

result = subprocess.run(
    [ccache, "-V"],
    stdout=subprocess.DEVNULL,
    stderr=subprocess.DEVNULL,
    check=False,
)
sys.exit(0 if result.returncode == 0 else 1)
PY
}

ns3_ccache_value="${NS3_CCACHE:-auto}"
case "$ns3_ccache_value" in
  auto)
    if ccache_usable; then
      ns3_ccache_value="ON"
    else
      ns3_ccache_value="OFF"
      echo "[ns3] disabling ccache because the installed ccache binary is not runnable"
    fi
    ;;
  1|ON|on|true|TRUE|yes|YES)
    ns3_ccache_value="ON"
    ;;
  0|OFF|off|false|FALSE|no|NO)
    ns3_ccache_value="OFF"
    ;;
  *)
    echo "[ns3] invalid NS3_CCACHE value: $ns3_ccache_value" >&2
    exit 1
    ;;
esac

cmake_args=(
  -S "$NS3_ROOT"
  -B "$BUILD_DIR"
  -G Ninja
  -DNS3_WARNINGS_AS_ERRORS=OFF
  -DNS3_EXAMPLES=OFF
  -DNS3_TESTS=OFF
  -DNS3_CCACHE="$ns3_ccache_value"
  -DNS3_OUTPUT_DIRECTORY="$OUTPUT_DIR"
  -DNS3_ENABLED_MODULES="$ENABLED_MODULES"
)

echo "[ns3] configuring focused build in $BUILD_DIR"
cmake "${cmake_args[@]}"

echo "[ns3] building with $NPROC jobs"
cmake --build "$BUILD_DIR" -j "$NPROC" \
  --target scratch_uav-secure-lte scratch_uav-secure-nr
