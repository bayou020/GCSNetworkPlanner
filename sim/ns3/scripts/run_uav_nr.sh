#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
NS3_BASE_DIR="${NS3_BASE_DIR:-$ROOT_DIR/.deps}"
NS3_ROOT="${NS3_ROOT:-$NS3_BASE_DIR/ns-allinone-3.47/ns-3.47}"
NS3_OUTPUT_DIR="${NS3_OUTPUT_DIR:-$NS3_ROOT/build-ran}"
RESULTS_DIR="${RESULTS_DIR:-$ROOT_DIR/sim/ns3/results}"

UAVS="${UAVS:-100}"
BASE_STATIONS="${BASE_STATIONS:-4}"
SIM_TIME="${SIM_TIME:-60}"
SECURITY="${SECURITY:-wireguard}"
CSV_PATH="${CSV_PATH:-$RESULTS_DIR/uav-secure-nr.csv}"
NS3_NR_BS_HEIGHT="${NS3_NR_BS_HEIGHT:-35}"
NS3_NR_DISTANCE="${NS3_NR_DISTANCE:-500}"
NS3_NR_SCENARIO_WIDTH="${NS3_NR_SCENARIO_WIDTH:-1200}"
NS3_NR_SCENARIO_HEIGHT="${NS3_NR_SCENARIO_HEIGHT:-1200}"
NS3_NR_FREQUENCY="${NS3_NR_FREQUENCY:-3500000000}"
NS3_NR_BANDWIDTH="${NS3_NR_BANDWIDTH:-40000000}"
NS3_NR_NUMEROLOGY="${NS3_NR_NUMEROLOGY:-1}"
NS3_NR_TX_POWER="${NS3_NR_TX_POWER:-43}"
NS3_TELEMETRY_PAYLOAD="${NS3_TELEMETRY_PAYLOAD:-180}"
NS3_TELEMETRY_INTERVAL_MS="${NS3_TELEMETRY_INTERVAL_MS:-100}"
NS3_CONTROL_PAYLOAD="${NS3_CONTROL_PAYLOAD:-96}"
NS3_CONTROL_INTERVAL_MS="${NS3_CONTROL_INTERVAL_MS:-500}"
LIVE="${LIVE:-0}"
LIVE_HOST="${LIVE_HOST:-127.0.0.1}"
LIVE_PORT="${LIVE_PORT:-${NS3_SIM_PORT:-45454}}"
LIVE_MIRROR_HOST="${LIVE_MIRROR_HOST:-}"
LIVE_MIRROR_PORT="${LIVE_MIRROR_PORT:-0}"
LIVE_INTERVAL_MS="${LIVE_INTERVAL_MS:-1000}"
MOBILITY="${MOBILITY:-0}"
MOBILITY_RADIUS="${MOBILITY_RADIUS:-70}"
ORIGIN_LAT="${ORIGIN_LAT:-39.904459}"
ORIGIN_LON="${ORIGIN_LON:-116.406847}"

"$ROOT_DIR/sim/ns3/scripts/build_ns3.sh"
mkdir -p "$RESULTS_DIR"

EXECUTABLE="$(find "$NS3_OUTPUT_DIR" -type f -executable -name '*uav-secure-nr*' | head -n 1)"
if [[ -z "$EXECUTABLE" ]]; then
  echo "[ns3] could not find built NR scenario executable in $NS3_OUTPUT_DIR" >&2
  exit 1
fi

echo "[ns3] running NR scenario: uavs=$UAVS gnbs=$BASE_STATIONS security=$SECURITY"
ARGS=(
  --uavs="$UAVS"
  --baseStations="$BASE_STATIONS"
  --simTime="$SIM_TIME"
  --security="$SECURITY"
  --csv="$CSV_PATH"
  --bsHeight="$NS3_NR_BS_HEIGHT"
  --distance="$NS3_NR_DISTANCE"
  --scenarioWidth="$NS3_NR_SCENARIO_WIDTH"
  --scenarioHeight="$NS3_NR_SCENARIO_HEIGHT"
  --frequency="$NS3_NR_FREQUENCY"
  --bandwidth="$NS3_NR_BANDWIDTH"
  --numerology="$NS3_NR_NUMEROLOGY"
  --txPower="$NS3_NR_TX_POWER"
  --telemetryPayload="$NS3_TELEMETRY_PAYLOAD"
  --telemetryIntervalMs="$NS3_TELEMETRY_INTERVAL_MS"
  --controlPayload="$NS3_CONTROL_PAYLOAD"
  --controlIntervalMs="$NS3_CONTROL_INTERVAL_MS"
)

if [[ "$LIVE" == "1" ]]; then
  ARGS+=(
    --live=1
    --liveHost="$LIVE_HOST"
    --livePort="$LIVE_PORT"
    --liveMirrorHost="$LIVE_MIRROR_HOST"
    --liveMirrorPort="$LIVE_MIRROR_PORT"
    --liveIntervalMs="$LIVE_INTERVAL_MS"
    --mobility="$MOBILITY"
    --mobilityRadius="$MOBILITY_RADIUS"
    --originLat="$ORIGIN_LAT"
    --originLon="$ORIGIN_LON"
  )
fi

"$EXECUTABLE" "${ARGS[@]}" "$@"
