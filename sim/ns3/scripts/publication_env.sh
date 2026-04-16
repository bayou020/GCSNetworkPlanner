#!/usr/bin/env bash

derive_publication_defaults() {
  local rat="$1"
  local uavs="$2"
  local security="$3"
  local mobility="$4"
  local default_log_root="$5"
  local motion_mode="static"

  if [[ "$mobility" == "1" ]]; then
    motion_mode="mob"
  fi

  export NP_RAT="${NP_RAT:-$rat}"
  export NP_SECURITY_PROFILE="${NP_SECURITY_PROFILE:-$security}"
  export NP_SYNC_METHOD="${NP_SYNC_METHOD:-unspecified}"
  export NP_RNG_RUN="${NP_RNG_RUN:-1}"
  export NP_LOG_ROOT="${NP_LOG_ROOT:-$default_log_root}"
  export NP_SCENARIO_ID="${NP_SCENARIO_ID:-sim-${rat}-${uavs}-${motion_mode}-${security}-default}"
  export NP_RUN_ID="${NP_RUN_ID:-$(date -u +%Y%m%dT%H%M%SZ)}"
}

derive_live_visualization_defaults() {
  local uavs="$1"
  local with_rpi="${2:-0}"
  local dense_threshold=50

  if [[ -z "${LIVE_INTERVAL_MS:-}" ]]; then
    if (( uavs >= dense_threshold )); then
      export LIVE_INTERVAL_MS=75
    else
      export LIVE_INTERVAL_MS=100
    fi
  fi

  if [[ -z "${NP_GCS_NS3_UI_UPDATE_MS:-}" ]]; then
    if (( uavs >= dense_threshold )); then
      export NP_GCS_NS3_UI_UPDATE_MS=50
    else
      export NP_GCS_NS3_UI_UPDATE_MS=75
    fi
  fi

  if [[ "$with_rpi" != "1" ]]; then
    return
  fi

  if [[ -z "${NPRPI_NS3_GPS_INTERVAL_MS:-}" ]]; then
    if (( uavs >= dense_threshold )); then
      export NPRPI_NS3_GPS_INTERVAL_MS=100
    else
      export NPRPI_NS3_GPS_INTERVAL_MS=125
    fi
  fi

  if [[ -z "${NPRPI_NS3_ATTITUDE_INTERVAL_MS:-}" ]]; then
    if (( uavs >= dense_threshold )); then
      export NPRPI_NS3_ATTITUDE_INTERVAL_MS=100
    else
      export NPRPI_NS3_ATTITUDE_INTERVAL_MS=125
    fi
  fi
}

run_log_dir() {
  printf '%s/raw/%s/%s' "${NP_LOG_ROOT:?}" "${NP_SCENARIO_ID:?}" "${NP_RUN_ID:?}"
}

default_ns3_flow_csv() {
  local rat="$1"
  printf '%s/ns3_%s_flow_monitor.csv' "$(run_log_dir)" "$rat"
}

default_ns3_link_model_csv() {
  local rat="$1"
  printf '%s/ns3_%s_link_model.csv' "$(run_log_dir)" "$rat"
}

default_ns3_metadata_json() {
  local rat="$1"
  printf '%s/ns3_%s_metadata.json' "$(run_log_dir)" "$rat"
}

ensure_run_log_dir() {
  mkdir -p "$(run_log_dir)"
}
