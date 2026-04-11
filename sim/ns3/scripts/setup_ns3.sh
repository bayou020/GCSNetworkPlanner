#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
NS3_ARCHIVE="${NS3_ARCHIVE:-$HOME/Downloads/ns-allinone-3.47.tar.bz2}"
NS3_BASE_DIR="${NS3_BASE_DIR:-$ROOT_DIR/.deps}"
NS3_ROOT="${NS3_ROOT:-$NS3_BASE_DIR/ns-allinone-3.47/ns-3.47}"

echo "[ns3] root: $ROOT_DIR"
echo "[ns3] archive: $NS3_ARCHIVE"

if [[ ! -f "$NS3_ARCHIVE" ]]; then
  echo "[ns3] archive not found: $NS3_ARCHIVE" >&2
  exit 1
fi

mkdir -p "$NS3_BASE_DIR"

if [[ ! -d "$NS3_ROOT" ]]; then
  echo "[ns3] extracting ns-allinone-3.47 into $NS3_BASE_DIR"
  tar -xjf "$NS3_ARCHIVE" -C "$NS3_BASE_DIR"
fi

if [[ ! -d "$NS3_ROOT/scratch" ]]; then
  echo "[ns3] invalid ns-3 root: $NS3_ROOT" >&2
  exit 1
fi

cp "$ROOT_DIR/sim/ns3/scenarios/uav-secure-lte.cc" "$NS3_ROOT/scratch/uav-secure-lte.cc"
cp "$ROOT_DIR/sim/ns3/scenarios/uav-secure-nr.cc" "$NS3_ROOT/scratch/uav-secure-nr.cc"

echo "[ns3] prepared scratch scenarios in $NS3_ROOT/scratch"
