#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
NETWORKPLANNER_RPI_ROOT="${NETWORKPLANNER_RPI_ROOT:-$ROOT_DIR/../NetworkPlannerRpi}"
RPI_SCRIPT="${RPI_SCRIPT:-$NETWORKPLANNER_RPI_ROOT/scripts/run_ns3_live.sh}"
export NS3_SIM_RPI_PORT="${NS3_SIM_RPI_PORT:-45455}"

if [[ ! -x "$RPI_SCRIPT" ]]; then
  echo "[stack] expected RPi live bridge script at $RPI_SCRIPT" >&2
  exit 1
fi

"$RPI_SCRIPT" &
RPI_PID=$!

cleanup() {
  if kill -0 "$RPI_PID" >/dev/null 2>&1; then
    kill "$RPI_PID" >/dev/null 2>&1 || true
    wait "$RPI_PID" >/dev/null 2>&1 || true
  fi
}

trap cleanup EXIT INT TERM

sleep 1
LIVE=1 \
MOBILITY="${MOBILITY:-1}" \
LIVE_MIRROR_HOST="${LIVE_MIRROR_HOST:-127.0.0.1}" \
LIVE_MIRROR_PORT="${LIVE_MIRROR_PORT:-$NS3_SIM_RPI_PORT}" \
"${ROOT_DIR}/sim/ns3/scripts/run_uav_lte.sh" "$@"
