#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
NS3_BASE_DIR="${NS3_BASE_DIR:-$ROOT_DIR/.deps}"
NS3_ROOT="${NS3_ROOT:-$NS3_BASE_DIR/ns-allinone-3.47/ns-3.47}"
NPROC="${NPROC:-$(nproc)}"
BUILD_DIR="${NS3_BUILD_DIR:-$NS3_ROOT/cmake-ran-cache}"
OUTPUT_DIR="${NS3_OUTPUT_DIR:-$NS3_ROOT/build-ran}"
ENABLED_MODULES="${NS3_ENABLED_MODULES:-antenna;applications;buildings;core;flow-monitor;internet;lte;mobility;network;nr;point-to-point;propagation;spectrum;stats}"

"$ROOT_DIR/sim/ns3/scripts/setup_ns3.sh"

cd "$NS3_ROOT"

echo "[ns3] configuring focused build in $BUILD_DIR"
cmake -S "$NS3_ROOT" -B "$BUILD_DIR" -G Ninja \
  -DNS3_WARNINGS_AS_ERRORS=OFF \
  -DNS3_EXAMPLES=OFF \
  -DNS3_TESTS=OFF \
  -DNS3_OUTPUT_DIRECTORY="$OUTPUT_DIR" \
  -DNS3_ENABLED_MODULES="$ENABLED_MODULES"

echo "[ns3] building with $NPROC jobs"
cmake --build "$BUILD_DIR" -j "$NPROC" \
  --target scratch_uav-secure-lte scratch_uav-secure-nr
