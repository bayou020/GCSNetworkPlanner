#!/usr/bin/env python3
"""Launch the LTE security-sweep calibration matrix run-by-run.

Reads a campaign config (default: analysis/templates/lte_security_sweep_campaign.template.json),
walks the `calibration` split, and invokes sim/ns3/scripts/run_uav_lte.sh once per
declared repetition. Hold-out families are skipped entirely.

Idempotency:
  A repetition is considered satisfied when its scenario_id directory already
  contains at least `--required-reps` run_ids whose `ns3_lte_flow_monitor.csv`
  exists. Already-satisfied families are skipped.

Filters:
  --filter-profile, --filter-fleet, --filter-motion narrow the run set; useful
  for resuming a partial campaign.

Sentinel-aware:
  Refuses to run if the scenario_id appears in the campaign hold-out lockfile.
"""
from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Iterable

REPO_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_CONFIG = REPO_ROOT / "analysis" / "templates" / "lte_security_sweep_campaign.template.json"
DEFAULT_LOCK = REPO_ROOT / "logs" / "analysis" / "holdout_lock.json"
RUNNER = REPO_ROOT / "sim" / "ns3" / "scripts" / "run_uav_lte.sh"
FAMILY_RE = re.compile(r"^lte-(?P<fleet>\d+)-(?P<motion>static|mob)-(?P<profile>none|tls|wireguard|openvpn)-secsweep$")

# fleet → base-station count. Mirrors the existing dataset's `100x10` shape.
BASE_STATIONS = {1: 1, 10: 2, 50: 4, 100: 10}


@dataclass(frozen=True)
class Cell:
    family_id: str
    fleet: int
    motion: str  # "static" | "mob"
    profile: str
    repetition: int

    @property
    def scenario_id(self) -> str:
        return f"sim-{self.family_id}"

    @property
    def mobility_flag(self) -> str:
        return "1" if self.motion == "mob" else "0"


def utc_stamp() -> str:
    return datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")


def parse_family(fid: str) -> tuple[int, str, str]:
    m = FAMILY_RE.match(fid)
    if not m:
        raise ValueError(f"family_id {fid!r} does not match expected pattern")
    return int(m["fleet"]), m["motion"], m["profile"]


def load_cells(config_path: Path) -> list[Cell]:
    config = json.loads(config_path.read_text(encoding="utf-8"))
    cells: list[Cell] = []
    for run in config.get("runs", []):
        if run.get("split") != "calibration":
            continue
        fid = run["family_id"]
        fleet, motion, profile = parse_family(fid)
        cells.append(
            Cell(
                family_id=fid,
                fleet=fleet,
                motion=motion,
                profile=profile,
                repetition=int(run["repetition"]),
            )
        )
    return cells


def load_holdout_scenarios(lock_path: Path) -> set[str]:
    if not lock_path.exists():
        return set()
    lock = json.loads(lock_path.read_text(encoding="utf-8"))
    return set(lock.get("holdout_scenario_ids", []))


def finished_runs(scenario_dir: Path) -> int:
    if not scenario_dir.is_dir():
        return 0
    return sum(
        1
        for run_dir in scenario_dir.iterdir()
        if run_dir.is_dir() and (run_dir / "ns3_lte_flow_monitor.csv").exists()
    )


def apply_filters(cells: list[Cell], args: argparse.Namespace) -> list[Cell]:
    selected = cells
    if args.filter_profile:
        wanted = set(args.filter_profile)
        selected = [c for c in selected if c.profile in wanted]
    if args.filter_fleet:
        wanted_int = {int(x) for x in args.filter_fleet}
        selected = [c for c in selected if c.fleet in wanted_int]
    if args.filter_motion:
        wanted = set(args.filter_motion)
        selected = [c for c in selected if c.motion in wanted]
    return selected


def group_by_family(cells: Iterable[Cell]) -> dict[str, list[Cell]]:
    grouped: dict[str, list[Cell]] = {}
    for cell in cells:
        grouped.setdefault(cell.family_id, []).append(cell)
    for v in grouped.values():
        v.sort(key=lambda c: c.repetition)
    return grouped


def build_env(cell: Cell, run_id: str, skip_build: bool, sim_time: int) -> dict[str, str]:
    env = os.environ.copy()
    env.update(
        {
            "UAVS": str(cell.fleet),
            "BASE_STATIONS": str(BASE_STATIONS[cell.fleet]),
            "SIM_TIME": str(sim_time),
            "SECURITY": cell.profile,
            "MOBILITY": cell.mobility_flag,
            "NP_SCENARIO_ID": cell.scenario_id,
            "NP_RUN_ID": run_id,
            "NP_RAT": "lte",
            "NP_SECURITY_PROFILE": cell.profile,
            "NP_EXECUTION_MODE": "pure_simulator",
            "NP_RNG_RUN": str(cell.repetition),
            "NS3_SKIP_BUILD": "1" if skip_build else "0",
        }
    )
    return env


def run_cell(cell: Cell, run_id: str, skip_build: bool, sim_time: int, dry_run: bool) -> int:
    env = build_env(cell, run_id, skip_build, sim_time)
    cmd = [str(RUNNER)]
    msg = (
        f"[batch] {cell.family_id} rep={cell.repetition} run_id={run_id} "
        f"uavs={cell.fleet} bs={BASE_STATIONS[cell.fleet]} security={cell.profile} mobility={cell.mobility_flag}"
    )
    print(msg, flush=True)
    if dry_run:
        return 0
    proc = subprocess.run(cmd, env=env)
    return proc.returncode


def write_manifest(manifest_path: Path, attempted: list[dict]) -> None:
    manifest_path.parent.mkdir(parents=True, exist_ok=True)
    manifest_path.write_text(
        json.dumps(
            {
                "generated_at_utc": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
                "attempted": attempted,
            },
            indent=2,
        )
        + "\n",
        encoding="utf-8",
    )


def parse_args(argv: Iterable[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--campaign-config", type=Path, default=DEFAULT_CONFIG)
    parser.add_argument("--holdout-lock", type=Path, default=DEFAULT_LOCK)
    parser.add_argument("--required-reps", type=int, default=3, help="Skip a family once this many finished runs exist.")
    parser.add_argument("--sim-time", type=int, default=60, help="ns-3 simulation duration in seconds.")
    parser.add_argument("--filter-profile", action="append", choices=["none", "tls", "wireguard", "openvpn"])
    parser.add_argument("--filter-fleet", action="append", choices=["1", "10", "100"])
    parser.add_argument("--filter-motion", action="append", choices=["static", "mob"])
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--stop-on-failure", action="store_true", help="Exit immediately on any failed run.")
    parser.add_argument("--manifest", type=Path, default=REPO_ROOT / "logs" / "analysis" / "security_sweep_batch_manifest.json")
    return parser.parse_args(argv)


def main(argv: Iterable[str] | None = None) -> int:
    args = parse_args(argv)
    if not RUNNER.exists():
        print(f"FAIL: runner not found at {RUNNER}", file=sys.stderr)
        return 2

    cells = load_cells(args.campaign_config)
    cells = apply_filters(cells, args)
    if not cells:
        print("no cells selected after filtering")
        return 0

    holdout = load_holdout_scenarios(args.holdout_lock)
    families = group_by_family(cells)

    raw_root = REPO_ROOT / "logs" / "raw"
    attempted: list[dict] = []
    failures = 0
    builds_skipped = False

    for family_id, fam_cells in sorted(families.items()):
        scenario_id = fam_cells[0].scenario_id
        if scenario_id in holdout:
            print(f"[batch] SKIP hold-out scenario {scenario_id}")
            continue

        already = finished_runs(raw_root / scenario_id)
        needed = max(0, args.required_reps - already)
        if needed == 0:
            print(f"[batch] SKIP {family_id}: already has {already} finished run(s) (>= {args.required_reps})")
            continue

        print(f"[batch] {family_id}: {already} finished, need {needed} more")
        for cell in fam_cells[:needed]:
            run_id = utc_stamp()
            rc = run_cell(cell, run_id, skip_build=builds_skipped, sim_time=args.sim_time, dry_run=args.dry_run)
            attempted.append(
                {
                    "scenario_id": scenario_id,
                    "family_id": family_id,
                    "repetition": cell.repetition,
                    "run_id": run_id,
                    "exit_code": rc,
                }
            )
            if rc != 0:
                failures += 1
                if args.stop_on_failure:
                    write_manifest(args.manifest, attempted)
                    print(f"[batch] FAIL: cell {family_id} rep={cell.repetition} exit={rc}; stopping", file=sys.stderr)
                    return 1
            elif not args.dry_run:
                builds_skipped = True

    write_manifest(args.manifest, attempted)
    print(f"[batch] done: attempted={len(attempted)} failures={failures}")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
