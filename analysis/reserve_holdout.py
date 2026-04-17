#!/usr/bin/env python3
"""Reserve and audit hold-out scenarios for a campaign config.

The whole point: hold-out scenarios must be selected and frozen *before* any
calibration tuning happens, otherwise reviewers will (correctly) reject the
"hold-out" framing as post-hoc cherry picking.

Two subcommands:

  reserve  Reads a campaign config, writes a lockfile naming every hold-out
           scenario_id and the SHA-256 of the campaign config at lock time.
           Optionally drops sentinel marker files inside each hold-out
           scenario's raw-log directory so accidental reuse is visible.

  audit    Re-reads the same campaign config and verifies (a) the hold-out
           scenario set is unchanged, (b) the config's SHA-256 still matches
           the locked value, and (c) no extra runs have appeared in the
           hold-out scenario directories that were not declared in the config.

Exit codes:
  0  reservation written / audit passed
  1  audit failed (drift detected)
  2  invalid input
"""
from __future__ import annotations

import argparse
import hashlib
import json
import sys
from datetime import datetime, timezone
from pathlib import Path
from typing import Iterable

LOCKFILE_NAME = "holdout_lock.json"
SENTINEL_NAME = ".holdout_reserved.json"


def utc_now() -> str:
    return datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


def sha256_text(text: str) -> str:
    return hashlib.sha256(text.encode("utf-8")).hexdigest()


def read_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def write_json(path: Path, payload: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")


def collect_holdout_families(config: dict) -> list[dict]:
    return list(config.get("families", {}).get("holdout", []))


def holdout_scenario_ids(config: dict) -> list[str]:
    ids: set[str] = set()
    for fam in collect_holdout_families(config):
        for key in ("field_scenario_id", "sim_scenario_id"):
            sid = fam.get(key)
            if sid:
                ids.add(sid)
    return sorted(ids)


def lockfile_path(analysis_root: Path) -> Path:
    return analysis_root / LOCKFILE_NAME


def existing_run_ids(scenario_dir: Path) -> list[str]:
    if not scenario_dir.is_dir():
        return []
    return sorted(p.name for p in scenario_dir.iterdir() if p.is_dir())


def declared_run_ids_per_scenario(config: dict) -> dict[str, set[str]]:
    """Map scenario_id -> set of declared (non-empty) run_ids on hold-out runs."""
    declared: dict[str, set[str]] = {}
    family_lookup = {
        f["family_id"]: f for f in collect_holdout_families(config)
    }
    for run in config.get("runs", []):
        if run.get("split") != "holdout":
            continue
        fam = family_lookup.get(run.get("family_id"))
        if not fam:
            continue
        for sid_key, run_key in (
            ("field_scenario_id", "field_run_id"),
            ("sim_scenario_id", "sim_run_id"),
        ):
            sid = fam.get(sid_key)
            rid = (run.get(run_key) or "").strip()
            if sid and rid:
                declared.setdefault(sid, set()).add(rid)
            elif sid:
                declared.setdefault(sid, set())
    return declared


def cmd_reserve(args: argparse.Namespace) -> int:
    config_path = args.campaign_config.resolve()
    config_text = config_path.read_text(encoding="utf-8")
    config = json.loads(config_text)

    families = collect_holdout_families(config)
    if not families:
        print(
            f"refusing to reserve: campaign {config_path} declares no hold-out families",
            file=sys.stderr,
        )
        return 2

    analysis_root = Path(config.get("analysis_root", "logs/analysis"))
    raw_root = Path(config.get("raw_root", "logs/raw"))
    sids = holdout_scenario_ids(config)

    lock = {
        "lock_version": 1,
        "campaign_name": config.get("campaign_name"),
        "campaign_config_path": str(config_path),
        "campaign_config_sha256": sha256_text(config_text),
        "reserved_at_utc": utc_now(),
        "holdout_family_ids": sorted(f["family_id"] for f in families),
        "holdout_scenario_ids": sids,
        "raw_root": str(raw_root),
    }

    lock_path = lockfile_path(analysis_root)
    if lock_path.exists() and not args.force:
        existing = read_json(lock_path)
        if existing.get("campaign_config_sha256") == lock["campaign_config_sha256"]:
            print(f"hold-out lock already exists and matches: {lock_path}")
            return 0
        print(
            f"refusing to overwrite existing lock at {lock_path}; pass --force to replace",
            file=sys.stderr,
        )
        return 2

    write_json(lock_path, lock)

    sentinel_count = 0
    for sid in sids:
        sentinel_dir = raw_root / sid
        sentinel_dir.mkdir(parents=True, exist_ok=True)
        sentinel = sentinel_dir / SENTINEL_NAME
        write_json(
            sentinel,
            {
                "scenario_id": sid,
                "campaign_name": config.get("campaign_name"),
                "reserved_at_utc": lock["reserved_at_utc"],
                "lockfile": str(lock_path),
                "rule": "Do not run, tune, or analyze against this scenario until the calibration / sweep is finalized.",
            },
        )
        sentinel_count += 1

    print(
        f"reserved {len(sids)} hold-out scenarios; lockfile {lock_path}; "
        f"{sentinel_count} sentinel marker(s) written under {raw_root}"
    )
    return 0


def cmd_audit(args: argparse.Namespace) -> int:
    config_path = args.campaign_config.resolve()
    config_text = config_path.read_text(encoding="utf-8")
    config = json.loads(config_text)

    analysis_root = Path(config.get("analysis_root", "logs/analysis"))
    raw_root = Path(config.get("raw_root", "logs/raw"))
    lock_path = lockfile_path(analysis_root)

    if not lock_path.exists():
        print(f"FAIL: no lockfile at {lock_path}", file=sys.stderr)
        return 1

    lock = read_json(lock_path)
    failures: list[str] = []

    if lock.get("campaign_config_sha256") != sha256_text(config_text):
        failures.append(
            "campaign config SHA-256 changed since reservation; "
            "the hold-out set may have been edited"
        )

    declared = sorted(set(lock.get("holdout_scenario_ids", [])))
    current = holdout_scenario_ids(config)
    if declared != current:
        failures.append(
            f"hold-out scenario set drift: locked={declared!r}, current={current!r}"
        )

    declared_runs = declared_run_ids_per_scenario(config)
    for sid in declared:
        scenario_dir = raw_root / sid
        present = set(existing_run_ids(scenario_dir))
        allowed = declared_runs.get(sid, set())
        unknown = sorted(present - allowed)
        if unknown:
            failures.append(
                f"hold-out scenario {sid} contains undeclared run_ids: {unknown}"
            )
        sentinel = scenario_dir / SENTINEL_NAME
        if scenario_dir.exists() and not sentinel.exists():
            failures.append(
                f"hold-out scenario {sid} is missing sentinel {SENTINEL_NAME}; "
                "directory may have been wiped"
            )

    if failures:
        print("audit FAILED:", file=sys.stderr)
        for line in failures:
            print(f"  - {line}", file=sys.stderr)
        return 1

    print(
        f"audit OK: {len(declared)} hold-out scenarios verified against lockfile {lock_path}"
    )
    return 0


def parse_args(argv: Iterable[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="command", required=True)

    p_reserve = sub.add_parser("reserve", help="Lock the hold-out subset before any tuning.")
    p_reserve.add_argument("--campaign-config", required=True, type=Path)
    p_reserve.add_argument("--force", action="store_true", help="Overwrite an existing lockfile.")
    p_reserve.set_defaults(func=cmd_reserve)

    p_audit = sub.add_parser("audit", help="Verify the hold-out set has not drifted.")
    p_audit.add_argument("--campaign-config", required=True, type=Path)
    p_audit.set_defaults(func=cmd_audit)

    return parser.parse_args(argv)


def main(argv: Iterable[str] | None = None) -> int:
    args = parse_args(argv)
    return args.func(args)


if __name__ == "__main__":
    raise SystemExit(main())
