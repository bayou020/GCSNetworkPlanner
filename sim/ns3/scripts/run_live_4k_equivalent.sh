#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"

RAT="${RAT:-lte}"
START_GCS="${START_GCS:-1}"
WITH_RPI="${WITH_RPI:-0}"
UAVS="${UAVS:-20}"
BASE_STATIONS="${BASE_STATIONS:-4}"
SIM_TIME="${SIM_TIME:-120}"
SECURITY="${SECURITY:-openvpn}"
MOBILITY="${MOBILITY:-1}"
VIDEO_STREAM_UAVS="${VIDEO_STREAM_UAVS:-1}"
VIDEO_BITRATE_MBPS="${VIDEO_BITRATE_MBPS:-25}"
VIDEO_PAYLOAD_BYTES="${VIDEO_PAYLOAD_BYTES:-1400}"

motion_mode="static"
if [[ "$MOBILITY" == "1" ]]; then
    motion_mode="mob"
fi

export NP_SCENARIO_ID="${NP_SCENARIO_ID:-sim-${RAT}-${UAVS}-${motion_mode}-${SECURITY}-4k-equiv-${VIDEO_STREAM_UAVS}uav}"

echo "[stack] launching 4K-equivalent ns-3 traffic profile"
echo "[stack] rat=$RAT uavs=$UAVS base_stations=$BASE_STATIONS sim_time=$SIM_TIME security=$SECURITY video_stream_uavs=$VIDEO_STREAM_UAVS video_bitrate_mbps=$VIDEO_BITRATE_MBPS video_payload_bytes=$VIDEO_PAYLOAD_BYTES"

START_GCS="$START_GCS" \
WITH_RPI="$WITH_RPI" \
RAT="$RAT" \
UAVS="$UAVS" \
BASE_STATIONS="$BASE_STATIONS" \
SIM_TIME="$SIM_TIME" \
SECURITY="$SECURITY" \
MOBILITY="$MOBILITY" \
NS3_VIDEO_STREAM_UAVS="$VIDEO_STREAM_UAVS" \
NS3_VIDEO_BITRATE_MBPS="$VIDEO_BITRATE_MBPS" \
NS3_VIDEO_PAYLOAD_BYTES="$VIDEO_PAYLOAD_BYTES" \
"$ROOT_DIR/sim/ns3/scripts/run_live_network_planner_100x4.sh" "$@"
