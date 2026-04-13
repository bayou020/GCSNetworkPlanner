#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/sim/ns3/scripts/publication_env.sh"
NETWORKPLANNER_RPI_ROOT="${NETWORKPLANNER_RPI_ROOT:-$ROOT_DIR/../NetworkPlannerRpi}"
RPI_SCRIPT="${RPI_SCRIPT:-$NETWORKPLANNER_RPI_ROOT/scripts/run_ns3_live.sh}"
export NS3_SIM_RPI_PORT="${NS3_SIM_RPI_PORT:-45455}"

derive_publication_defaults "nr" "${UAVS:-100}" "${SECURITY:-wireguard}" "${MOBILITY:-1}" "$ROOT_DIR/logs"

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
NS3_NR_BS_HEIGHT="${NS3_NR_BS_HEIGHT:-35}" \
NS3_NR_DISTANCE="${NS3_NR_DISTANCE:-500}" \
NS3_NR_SCENARIO_WIDTH="${NS3_NR_SCENARIO_WIDTH:-1200}" \
NS3_NR_SCENARIO_HEIGHT="${NS3_NR_SCENARIO_HEIGHT:-1200}" \
NS3_NR_FREQUENCY="${NS3_NR_FREQUENCY:-3500000000}" \
NS3_NR_BANDWIDTH="${NS3_NR_BANDWIDTH:-40000000}" \
NS3_NR_NUMEROLOGY="${NS3_NR_NUMEROLOGY:-1}" \
NS3_NR_TX_POWER="${NS3_NR_TX_POWER:-43}" \
NS3_TELEMETRY_PAYLOAD="${NS3_TELEMETRY_PAYLOAD:-180}" \
NS3_TELEMETRY_INTERVAL_MS="${NS3_TELEMETRY_INTERVAL_MS:-100}" \
NS3_CONTROL_PAYLOAD="${NS3_CONTROL_PAYLOAD:-96}" \
NS3_CONTROL_INTERVAL_MS="${NS3_CONTROL_INTERVAL_MS:-500}" \
LIVE_MIRROR_HOST="${LIVE_MIRROR_HOST:-127.0.0.1}" \
LIVE_MIRROR_PORT="${LIVE_MIRROR_PORT:-$NS3_SIM_RPI_PORT}" \
"${ROOT_DIR}/sim/ns3/scripts/run_uav_nr.sh" "$@"
