#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"

LIVE=1 MOBILITY="${MOBILITY:-1}" "${ROOT_DIR}/sim/ns3/scripts/run_uav_nr.sh" "$@"
