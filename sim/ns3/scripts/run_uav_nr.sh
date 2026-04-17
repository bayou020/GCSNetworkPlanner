#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/sim/ns3/scripts/publication_env.sh"
NS3_BASE_DIR="${NS3_BASE_DIR:-$ROOT_DIR/.deps}"
NS3_ROOT="${NS3_ROOT:-$NS3_BASE_DIR/ns-allinone-3.47/ns-3.47}"
NS3_OUTPUT_DIR="${NS3_OUTPUT_DIR:-$NS3_ROOT/build-ran}"
RESULTS_DIR="${RESULTS_DIR:-$ROOT_DIR/sim/ns3/results}"

find_built_executable() {
  local search_root="$1"
  local pattern="$2"
  local candidate=""

  while IFS= read -r candidate; do
    if [[ -x "$candidate" ]]; then
      printf '%s\n' "$candidate"
      return 0
    fi
  done < <(find "$search_root" -type f -name "$pattern")

  return 1
}

UAVS="${UAVS:-100}"
BASE_STATIONS="${BASE_STATIONS:-4}"
SIM_TIME="${SIM_TIME:-60}"
SECURITY="${SECURITY:-wireguard}"
NS3_NR_BS_HEIGHT="${NS3_NR_BS_HEIGHT:-35}"
NS3_NR_DISTANCE="${NS3_NR_DISTANCE:-500}"
NS3_NR_SCENARIO_WIDTH="${NS3_NR_SCENARIO_WIDTH:-1200}"
NS3_NR_SCENARIO_HEIGHT="${NS3_NR_SCENARIO_HEIGHT:-1200}"
NS3_NR_FREQUENCY="${NS3_NR_FREQUENCY:-3500000000}"
NS3_NR_BANDWIDTH="${NS3_NR_BANDWIDTH:-40000000}"
NS3_NR_NUMEROLOGY="${NS3_NR_NUMEROLOGY:-1}"
NS3_NR_TX_POWER="${NS3_NR_TX_POWER:-43}"
NS3_NR_UE_ANTENNA_ROWS="${NS3_NR_UE_ANTENNA_ROWS:-2}"
NS3_NR_UE_ANTENNA_COLUMNS="${NS3_NR_UE_ANTENNA_COLUMNS:-2}"
NS3_NR_GNB_ANTENNA_ROWS="${NS3_NR_GNB_ANTENNA_ROWS:-4}"
NS3_NR_GNB_ANTENNA_COLUMNS="${NS3_NR_GNB_ANTENNA_COLUMNS:-4}"
NS3_NR_BEAMFORMING_METHOD="${NS3_NR_BEAMFORMING_METHOD:-DirectPathBeamforming}"
NS3_TELEMETRY_PAYLOAD="${NS3_TELEMETRY_PAYLOAD:-180}"
NS3_TELEMETRY_INTERVAL_MS="${NS3_TELEMETRY_INTERVAL_MS:-100}"
NS3_CONTROL_PAYLOAD="${NS3_CONTROL_PAYLOAD:-96}"
NS3_CONTROL_INTERVAL_MS="${NS3_CONTROL_INTERVAL_MS:-500}"
NS3_VIDEO_PAYLOAD_BYTES="${NS3_VIDEO_PAYLOAD_BYTES:-1400}"
NS3_VIDEO_BITRATE_MBPS="${NS3_VIDEO_BITRATE_MBPS:-0}"
NS3_VIDEO_STREAM_UAVS="${NS3_VIDEO_STREAM_UAVS:-0}"
NS3_SKIP_BUILD="${NS3_SKIP_BUILD:-0}"
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

derive_publication_defaults "nr" "$UAVS" "$SECURITY" "$MOBILITY" "$ROOT_DIR/logs"
CSV_PATH="${CSV_PATH:-$(default_ns3_flow_csv nr)}"
LINK_MODEL_CSV_PATH="${LINK_MODEL_CSV_PATH:-$(default_ns3_link_model_csv nr)}"
METADATA_PATH="${METADATA_PATH:-$(default_ns3_metadata_json nr)}"

if [[ "$NS3_SKIP_BUILD" != "1" ]]; then
  "$ROOT_DIR/sim/ns3/scripts/build_ns3.sh"
else
  echo "[ns3] skipping build because NS3_SKIP_BUILD=1"
fi
mkdir -p "$RESULTS_DIR"
ensure_run_log_dir

EXECUTABLE="$(find_built_executable "$NS3_OUTPUT_DIR" '*uav-secure-nr*' || true)"
if [[ -z "$EXECUTABLE" ]]; then
  echo "[ns3] could not find built NR scenario executable in $NS3_OUTPUT_DIR" >&2
  exit 1
fi

echo "[ns3] running NR scenario: scenario=$NP_SCENARIO_ID run=$NP_RUN_ID uavs=$UAVS gnbs=$BASE_STATIONS security=$SECURITY"
ARGS=(
  --uavs="$UAVS"
  --baseStations="$BASE_STATIONS"
  --simTime="$SIM_TIME"
  --security="$SECURITY"
  --csv="$CSV_PATH"
  --linkModelCsv="$LINK_MODEL_CSV_PATH"
  --metadata="$METADATA_PATH"
  --scenarioId="$NP_SCENARIO_ID"
  --runId="$NP_RUN_ID"
  --executionMode="$NP_EXECUTION_MODE"
  --syncMethod="$NP_SYNC_METHOD"
  --RngRun="$NP_RNG_RUN"
  --bsHeight="$NS3_NR_BS_HEIGHT"
  --distance="$NS3_NR_DISTANCE"
  --scenarioWidth="$NS3_NR_SCENARIO_WIDTH"
  --scenarioHeight="$NS3_NR_SCENARIO_HEIGHT"
  --frequency="$NS3_NR_FREQUENCY"
  --bandwidth="$NS3_NR_BANDWIDTH"
  --numerology="$NS3_NR_NUMEROLOGY"
  --txPower="$NS3_NR_TX_POWER"
  --ueAntennaRows="$NS3_NR_UE_ANTENNA_ROWS"
  --ueAntennaColumns="$NS3_NR_UE_ANTENNA_COLUMNS"
  --gnbAntennaRows="$NS3_NR_GNB_ANTENNA_ROWS"
  --gnbAntennaColumns="$NS3_NR_GNB_ANTENNA_COLUMNS"
  --beamformingMethod="$NS3_NR_BEAMFORMING_METHOD"
  --telemetryPayload="$NS3_TELEMETRY_PAYLOAD"
  --telemetryIntervalMs="$NS3_TELEMETRY_INTERVAL_MS"
  --controlPayload="$NS3_CONTROL_PAYLOAD"
  --controlIntervalMs="$NS3_CONTROL_INTERVAL_MS"
  --videoPayload="$NS3_VIDEO_PAYLOAD_BYTES"
  --videoBitrateMbps="$NS3_VIDEO_BITRATE_MBPS"
  --videoUavs="$NS3_VIDEO_STREAM_UAVS"
  --mobility="$MOBILITY"
  --mobilityRadius="$MOBILITY_RADIUS"
)

if [[ -n "${NP_SYNC_OFFSET_MS:-}" ]]; then
  ARGS+=(--syncOffsetMs="$NP_SYNC_OFFSET_MS")
fi

if [[ -n "${NP_SYNC_NOTE:-}" ]]; then
  ARGS+=(--syncNote="$NP_SYNC_NOTE")
fi

if [[ "$LIVE" == "1" ]]; then
  ARGS+=(
    --live=1
    --liveHost="$LIVE_HOST"
    --livePort="$LIVE_PORT"
    --liveIntervalMs="$LIVE_INTERVAL_MS"
    --originLat="$ORIGIN_LAT"
    --originLon="$ORIGIN_LON"
  )

  if [[ -n "$LIVE_MIRROR_HOST" ]]; then
    ARGS+=(--liveMirrorHost="$LIVE_MIRROR_HOST")
  fi

  if [[ "$LIVE_MIRROR_PORT" != "0" ]]; then
    ARGS+=(--liveMirrorPort="$LIVE_MIRROR_PORT")
  fi
fi

"$EXECUTABLE" "${ARGS[@]}" "$@"
