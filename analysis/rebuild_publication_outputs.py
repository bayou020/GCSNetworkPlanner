#!/usr/bin/env python3
from __future__ import annotations

import argparse
from pathlib import Path
from typing import Any

from campaign_pipeline import load_campaign_config, process_campaign
from publication_pipeline import normalize_raw_run, summarize_normalized_run, utc_now, write_json


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Rebuild publication-derived outputs from raw logs and optional campaign configs."
    )
    parser.add_argument("--raw-root", type=Path, default=Path("logs/raw"))
    parser.add_argument("--normalized-root", type=Path, default=Path("logs/normalized"))
    parser.add_argument("--analysis-root", type=Path, default=Path("logs/analysis"))
    parser.add_argument("--campaign-config", action="append", type=Path, default=[])
    parser.add_argument("--write-parquet", action="store_true")
    return parser.parse_args()


def iter_raw_run_dirs(raw_root: Path) -> list[Path]:
    run_dirs: list[Path] = []
    if not raw_root.exists():
        return run_dirs
    for scenario_dir in sorted(path for path in raw_root.iterdir() if path.is_dir()):
        for run_dir in sorted(path for path in scenario_dir.iterdir() if path.is_dir()):
            if any(path.is_file() for path in run_dir.iterdir()):
                run_dirs.append(run_dir)
    return run_dirs


def main() -> int:
    args = parse_args()
    rebuilt_runs: list[dict[str, Any]] = []
    failed_runs: list[dict[str, str]] = []

    for raw_run_dir in iter_raw_run_dirs(args.raw_root):
        try:
            normalized_dir = normalize_raw_run(raw_run_dir, args.normalized_root, args.write_parquet)
            analysis_dir = summarize_normalized_run(normalized_dir, args.analysis_root)
            rebuilt_runs.append(
                {
                    "raw_run_dir": str(raw_run_dir.resolve()),
                    "normalized_dir": str(normalized_dir.resolve()),
                    "analysis_dir": str(analysis_dir.resolve()),
                }
            )
        except Exception as exc:  # pragma: no cover - integration path
            failed_runs.append(
                {
                    "raw_run_dir": str(raw_run_dir.resolve()),
                    "error": str(exc),
                }
            )

    rebuilt_campaigns: list[dict[str, str]] = []
    for config_path in args.campaign_config:
        config = load_campaign_config(config_path)
        output_dir = process_campaign(config, args.write_parquet)
        rebuilt_campaigns.append(
            {
                "campaign_config": str(config_path.resolve()),
                "campaign_output_dir": str(output_dir.resolve()),
            }
        )

    rebuild_dir = args.analysis_root / "rebuilds" / utc_now().replace(":", "").replace(".", "")
    rebuild_dir.mkdir(parents=True, exist_ok=True)
    summary = {
        "generated_at_utc": utc_now(),
        "raw_root": str(args.raw_root.resolve()),
        "normalized_root": str(args.normalized_root.resolve()),
        "analysis_root": str(args.analysis_root.resolve()),
        "rebuilt_run_count": len(rebuilt_runs),
        "failed_run_count": len(failed_runs),
        "rebuilt_runs": rebuilt_runs,
        "failed_runs": failed_runs,
        "rebuilt_campaigns": rebuilt_campaigns,
    }
    write_json(rebuild_dir / "rebuild_summary.json", summary)
    markdown_lines = [
        "# Rebuild Summary",
        "",
        f"- generated_at_utc: `{summary['generated_at_utc']}`",
        f"- rebuilt runs: `{summary['rebuilt_run_count']}`",
        f"- failed runs: `{summary['failed_run_count']}`",
        f"- rebuilt campaigns: `{len(rebuilt_campaigns)}`",
        "",
        "## Campaign Outputs",
        "",
    ]
    markdown_lines.extend(
        [f"- `{item['campaign_config']}` -> `{item['campaign_output_dir']}`" for item in rebuilt_campaigns]
        or ["- none"]
    )
    markdown_lines.extend(["", "## Failed Runs", ""])
    markdown_lines.extend(
        [f"- `{item['raw_run_dir']}`: {item['error']}" for item in failed_runs] or ["- none"]
    )
    (rebuild_dir / "rebuild_summary.md").write_text("\n".join(markdown_lines) + "\n", encoding="utf-8")
    print(rebuild_dir)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
