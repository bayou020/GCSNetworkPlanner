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
  export NP_LOG_ROOT="${NP_LOG_ROOT:-$default_log_root}"
  export NP_SCENARIO_ID="${NP_SCENARIO_ID:-sim-${rat}-${uavs}-${motion_mode}-${security}-default}"
  export NP_RUN_ID="${NP_RUN_ID:-$(date -u +%Y%m%dT%H%M%SZ)}"
}

run_log_dir() {
  printf '%s/raw/%s/%s' "${NP_LOG_ROOT:?}" "${NP_SCENARIO_ID:?}" "${NP_RUN_ID:?}"
}

default_ns3_flow_csv() {
  local rat="$1"
  printf '%s/ns3_%s_flow_monitor.csv' "$(run_log_dir)" "$rat"
}

default_ns3_metadata_json() {
  local rat="$1"
  printf '%s/ns3_%s_metadata.json' "$(run_log_dir)" "$rat"
}

ensure_run_log_dir() {
  mkdir -p "$(run_log_dir)"
}
