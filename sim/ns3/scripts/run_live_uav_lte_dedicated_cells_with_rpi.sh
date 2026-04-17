#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"

RAT="${RAT:-lte}"
WITH_RPI="${WITH_RPI:-1}"
START_GCS="${START_GCS:-1}"
UAVS="${UAVS:-100}"
BASE_STATIONS="${BASE_STATIONS:-$UAVS}"
SIM_TIME="${SIM_TIME:-120}"
SECURITY="${SECURITY:-wireguard}"
MOBILITY="${MOBILITY:-1}"
MOBILITY_RADIUS="${MOBILITY_RADIUS:-35}"
NS3_SKIP_BUILD="${NS3_SKIP_BUILD:-0}"

if [[ "$RAT" != "lte" ]]; then
    echo "This wrapper is LTE-only. Use RAT=lte or call the LTE helpers directly." >&2
    exit 1
fi

if (( BASE_STATIONS < UAVS )); then
    echo "Dedicated-cell mode requires BASE_STATIONS >= UAVS. Got UAVS=$UAVS BASE_STATIONS=$BASE_STATIONS." >&2
    exit 1
fi

motion_mode="static"
if [[ "$MOBILITY" == "1" ]]; then
    motion_mode="mob"
fi

export NP_SCENARIO_ID="${NP_SCENARIO_ID:-sim-lte-${UAVS}u-${BASE_STATIONS}bs-${motion_mode}-${SECURITY}-dedicated-cells}"
export NS3_LTE_INTERSITE_DISTANCE="${NS3_LTE_INTERSITE_DISTANCE:-1200}"
export NS3_LTE_COVERAGE_RADIUS="${NS3_LTE_COVERAGE_RADIUS:-120}"
export NS3_LTE_TX_POWER="${NS3_LTE_TX_POWER:-26}"

echo "[stack] dedicated-cell LTE mode: assigning one UAV per serving eNodeB when BASE_STATIONS >= UAVS"
echo "[stack] topology: UAVS=$UAVS BASE_STATIONS=$BASE_STATIONS INTERSITE_DISTANCE=$NS3_LTE_INTERSITE_DISTANCE COVERAGE_RADIUS=$NS3_LTE_COVERAGE_RADIUS MOBILITY_RADIUS=$MOBILITY_RADIUS"

START_GCS="$START_GCS" \
WITH_RPI="$WITH_RPI" \
RAT="lte" \
UAVS="$UAVS" \
BASE_STATIONS="$BASE_STATIONS" \
SIM_TIME="$SIM_TIME" \
SECURITY="$SECURITY" \
MOBILITY="$MOBILITY" \
MOBILITY_RADIUS="$MOBILITY_RADIUS" \
NS3_SKIP_BUILD="$NS3_SKIP_BUILD" \
"$ROOT_DIR/sim/ns3/scripts/run_live_network_planner_100x4.sh" "$@"
