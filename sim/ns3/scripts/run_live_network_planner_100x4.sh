#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/sim/ns3/scripts/publication_env.sh"

RAT="${RAT:-nr}"
START_GCS="${START_GCS:-1}"
WITH_RPI="${WITH_RPI:-0}"

UAVS="${UAVS:-100}"
BASE_STATIONS="${BASE_STATIONS:-4}"
SIM_TIME="${SIM_TIME:-120}"
SECURITY="${SECURITY:-wireguard}"
MOBILITY="${MOBILITY:-1}"
ORIGIN_LAT="${ORIGIN_LAT:-39.904459}"
ORIGIN_LON="${ORIGIN_LON:-116.406847}"
GCS_START_DELAY_SEC="${GCS_START_DELAY_SEC:-3}"

derive_live_visualization_defaults "$UAVS" "$WITH_RPI"

case "$RAT" in
    nr)
        if [[ "$WITH_RPI" == "1" ]]; then
            SCENARIO_SCRIPT="$ROOT_DIR/sim/ns3/scripts/run_live_uav_nr_with_rpi.sh"
        else
            SCENARIO_SCRIPT="$ROOT_DIR/sim/ns3/scripts/run_live_uav_nr.sh"
        fi
        ;;
    lte)
        if [[ "$WITH_RPI" == "1" ]]; then
            SCENARIO_SCRIPT="$ROOT_DIR/sim/ns3/scripts/run_live_uav_lte_with_rpi.sh"
        else
            SCENARIO_SCRIPT="$ROOT_DIR/sim/ns3/scripts/run_live_uav_lte.sh"
        fi
        ;;
    *)
        echo "Unsupported RAT '$RAT'. Use RAT=nr or RAT=lte." >&2
        exit 1
        ;;
esac

if [[ ! -x "$SCENARIO_SCRIPT" ]]; then
    echo "Expected scenario script at $SCENARIO_SCRIPT" >&2
    exit 1
fi

GCS_PID=""
cleanup_ran=0

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

cleanup() {
    if [[ "$cleanup_ran" == "1" ]]; then
        return
    fi
    cleanup_ran=1
    terminate_pid_tree "$GCS_PID"
}

on_signal() {
    cleanup
    exit 130
}

trap cleanup EXIT
trap on_signal INT TERM

if [[ "$START_GCS" == "1" ]]; then
    echo "[stack] starting GCSNetworkPlanner"
    NP_GCS_NS3_UI_UPDATE_MS="$NP_GCS_NS3_UI_UPDATE_MS" \
        "$ROOT_DIR/scripts/run.sh" &
    GCS_PID=$!
    sleep "$GCS_START_DELAY_SEC"
fi

echo "[stack] launching live $RAT scenario with $UAVS UAVs and $BASE_STATIONS base stations"
echo "[stack] overrides: SIM_TIME=$SIM_TIME SECURITY=$SECURITY MOBILITY=$MOBILITY LIVE_INTERVAL_MS=$LIVE_INTERVAL_MS WITH_RPI=$WITH_RPI"

UAVS="$UAVS" \
BASE_STATIONS="$BASE_STATIONS" \
SIM_TIME="$SIM_TIME" \
SECURITY="$SECURITY" \
MOBILITY="$MOBILITY" \
LIVE_INTERVAL_MS="$LIVE_INTERVAL_MS" \
ORIGIN_LAT="$ORIGIN_LAT" \
ORIGIN_LON="$ORIGIN_LON" \
"$SCENARIO_SCRIPT" "$@"
