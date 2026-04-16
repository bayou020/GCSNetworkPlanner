#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/sim/ns3/scripts/publication_env.sh"
NETWORKPLANNER_RPI_ROOT="${NETWORKPLANNER_RPI_ROOT:-$ROOT_DIR/../NetworkPlannerRpi}"
RPI_SCRIPT="${RPI_SCRIPT:-$NETWORKPLANNER_RPI_ROOT/scripts/run_ns3_live.sh}"
export NS3_SIM_RPI_PORT="${NS3_SIM_RPI_PORT:-45455}"

derive_publication_defaults "lte" "${UAVS:-100}" "${SECURITY:-wireguard}" "${MOBILITY:-1}" "$ROOT_DIR/logs"
derive_live_visualization_defaults "${UAVS:-100}" 1

ensure_no_existing_live_stack() {
  if [[ "${NP_ALLOW_CONCURRENT_LIVE_STACKS:-0}" == "1" ]]; then
    return
  fi

  local existing=""
  existing="$(pgrep -af 'NetworkPlannerRpi.*--simulation --ns3-live|ns3\.47-uav-secure-(lte|nr)-default .*--live=1' || true)"
  if [[ -n "$existing" ]]; then
    echo "[stack] another live ns-3/RPi stack is already running. Stop it before starting a new one." >&2
    echo "$existing" >&2
    exit 1
  fi
}

terminate_pid_tree() {
  local pid="${1:-}"
  [[ -z "$pid" ]] && return 0
  kill -0 "$pid" >/dev/null 2>&1 || return 0

  pkill -TERM -P "$pid" >/dev/null 2>&1 || true
  kill "$pid" >/dev/null 2>&1 || true

  local attempt=0
  while kill -0 "$pid" >/dev/null 2>&1 && (( attempt < 50 )); do
    sleep 0.1
    ((attempt+=1))
  done

  if kill -0 "$pid" >/dev/null 2>&1; then
    pkill -KILL -P "$pid" >/dev/null 2>&1 || true
    kill -KILL "$pid" >/dev/null 2>&1 || true
  fi

  wait "$pid" >/dev/null 2>&1 || true
}

if [[ ! -x "$RPI_SCRIPT" ]]; then
  echo "[stack] expected RPi live bridge script at $RPI_SCRIPT" >&2
  exit 1
fi

ensure_no_existing_live_stack

"$RPI_SCRIPT" &
RPI_PID=$!
cleanup_ran=0

cleanup() {
  if [[ "$cleanup_ran" == "1" ]]; then
    return
  fi
  cleanup_ran=1
  terminate_pid_tree "$RPI_PID"
}

on_signal() {
  cleanup
  exit 130
}

trap cleanup EXIT
trap on_signal INT TERM

sleep 1
LIVE=1 \
MOBILITY="${MOBILITY:-1}" \
NS3_LTE_INTERSITE_DISTANCE="${NS3_LTE_INTERSITE_DISTANCE:-750}" \
NS3_LTE_COVERAGE_RADIUS="${NS3_LTE_COVERAGE_RADIUS:-250}" \
NS3_LTE_DL_BANDWIDTH="${NS3_LTE_DL_BANDWIDTH:-50}" \
NS3_LTE_UL_BANDWIDTH="${NS3_LTE_UL_BANDWIDTH:-50}" \
NS3_LTE_TX_POWER="${NS3_LTE_TX_POWER:-30}" \
NS3_TELEMETRY_PAYLOAD="${NS3_TELEMETRY_PAYLOAD:-180}" \
NS3_TELEMETRY_INTERVAL_MS="${NS3_TELEMETRY_INTERVAL_MS:-100}" \
NS3_CONTROL_PAYLOAD="${NS3_CONTROL_PAYLOAD:-96}" \
NS3_CONTROL_INTERVAL_MS="${NS3_CONTROL_INTERVAL_MS:-500}" \
LIVE_MIRROR_HOST="${LIVE_MIRROR_HOST:-127.0.0.1}" \
LIVE_MIRROR_PORT="${LIVE_MIRROR_PORT:-$NS3_SIM_RPI_PORT}" \
"${ROOT_DIR}/sim/ns3/scripts/run_uav_lte.sh" "$@"
