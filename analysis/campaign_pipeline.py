#!/usr/bin/env python3
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from typing import Any

from publication_pipeline import (
    compare_field_and_sim,
    normalize_raw_run,
    read_json,
    summarize_normalized_run,
    utc_now,
    write_csv,
    write_json,
)

CAMPAIGN_PIPELINE_FINGERPRINT = hashlib.sha256(Path(__file__).read_bytes()).hexdigest()[:16]

PUBLICATION_GATE_DEFAULTS = {
    "require_all_expected_pairs": True,
    "require_all_holdout_pairs": True,
    "minimum_metric_overlap_per_pair": 3,
    "minimum_comparable_metric_overlap_per_pair": 3,
    "require_simulator_export_rows": True,
    "require_field_ground_truth_rows": True,
    "require_configured_field_sync": True,
    "require_real_hardware_field_execution": True,
    "require_sim_link_model_export": True,
}

DEV_GATE_DEFAULTS = {
    "require_all_expected_pairs": False,
    "require_all_holdout_pairs": False,
    "minimum_metric_overlap_per_pair": 1,
    "minimum_comparable_metric_overlap_per_pair": 0,
    "require_simulator_export_rows": True,
    "require_field_ground_truth_rows": False,
    "require_configured_field_sync": False,
    "require_real_hardware_field_execution": False,
    "require_sim_link_model_export": False,
}


def campaign_pipeline_metadata(stage: str) -> dict[str, Any]:
    return {
        "stage": stage,
        "fingerprint": CAMPAIGN_PIPELINE_FINGERPRINT,
        "script": Path(__file__).name,
    }


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Run batch publication analysis for a small calibration and hold-out campaign."
    )
    parser.add_argument("--campaign-config", required=True, type=Path)
    parser.add_argument("--write-parquet", action="store_true")
    return parser.parse_args()


def load_campaign_config(path: Path) -> dict[str, Any]:
    config = json.loads(path.read_text(encoding="utf-8"))
    config["_config_path"] = str(path.resolve())
    return config


def campaign_paths(config: dict[str, Any]) -> dict[str, Path]:
    return {
        "raw_root": Path(config.get("raw_root", "logs/raw")),
        "normalized_root": Path(config.get("normalized_root", "logs/normalized")),
        "analysis_root": Path(config.get("analysis_root", "logs/analysis")),
        "output_root": Path(config.get("output_root", "logs/analysis/campaigns")),
    }


def build_family_map(config: dict[str, Any]) -> dict[str, dict[str, Any]]:
    family_map: dict[str, dict[str, Any]] = {}
    for split in ("calibration", "holdout"):
        for family in config.get("families", {}).get(split, []):
            family_id = str(family["family_id"])
            if family_id in family_map:
                raise ValueError(f"duplicate family_id in campaign config: {family_id}")
            family_map[family_id] = {**family, "split": split}
    return family_map


def is_pending_value(value: Any) -> bool:
    if value is None:
        return True
    if isinstance(value, str):
        stripped = value.strip()
        if not stripped:
            return True
        if stripped.startswith("<") and stripped.endswith(">"):
            return True
        if stripped.upper() in {"TODO", "TBD", "PENDING"}:
            return True
    return False


def event_file_presence(raw_run_dir: Path) -> dict[str, bool]:
    files = {path.name for path in raw_run_dir.glob("*") if path.is_file()}
    return {
        "has_gcs_logs": {"gcs_events.jsonl", "gcs_metadata.json"}.issubset(files),
        "has_rpi_logs": {"rpi_bridge_events.jsonl", "rpi_bridge_metadata.json"}.issubset(files),
        "has_ns3_flow_monitor": any(name.endswith("_flow_monitor.csv") and name.startswith("ns3_") for name in files),
        "has_ns3_link_model": any(name.endswith("_link_model.csv") and name.startswith("ns3_") for name in files),
        "has_ns3_metadata": any(name.endswith("_metadata.json") and name.startswith("ns3_") for name in files),
        "has_ns3_exports": any(name.endswith("_flow_monitor.csv") and name.startswith("ns3_") for name in files)
        and any(name.endswith("_metadata.json") and name.startswith("ns3_") for name in files),
    }


def make_markdown_table(rows: list[dict[str, Any]], columns: list[tuple[str, str]]) -> str:
    lines = [
        "| " + " | ".join(label for _, label in columns) + " |",
        "| " + " | ".join("---" for _ in columns) + " |",
    ]
    for row in rows:
        rendered = []
        for key, _ in columns:
            value = row.get(key, "")
            rendered.append("" if value is None else str(value))
        lines.append("| " + " | ".join(rendered) + " |")
    return "\n".join(lines) + "\n"


def resolved_quality_gates(config: dict[str, Any], profile: str) -> dict[str, Any]:
    defaults = DEV_GATE_DEFAULTS if profile == "dev" else PUBLICATION_GATE_DEFAULTS
    overrides = config.get("dev_quality_gates", {}) if profile == "dev" else config.get("quality_gates", {})
    return {**defaults, **overrides}


def readiness_assessment_from_inventory(
    config: dict[str, Any], inventory_rows: list[dict[str, Any]], profile: str
) -> dict[str, Any]:
    gates = resolved_quality_gates(config, profile)
    require_all_expected_pairs = bool(gates.get("require_all_expected_pairs", True))
    require_all_holdout_pairs = bool(gates.get("require_all_holdout_pairs", True))
    minimum_metric_overlap = int(gates.get("minimum_metric_overlap_per_pair", 1))
    minimum_comparable_overlap = int(gates.get("minimum_comparable_metric_overlap_per_pair", 0))

    reasons: list[str] = []
    warnings: list[str] = []
    total_pairs = len(inventory_rows)
    completed_pairs = [row for row in inventory_rows if row["status"] == "complete"]
    holdout_rows = [row for row in inventory_rows if row["split"] == "holdout"]
    completed_holdout_rows = [row for row in holdout_rows if row["status"] == "complete"]

    if not completed_pairs:
        reasons.append("no complete pairs are available")

    if require_all_expected_pairs and len(completed_pairs) != total_pairs:
        reasons.append(
            f"expected {total_pairs} complete pairs, found {len(completed_pairs)} complete pairs"
        )
    if require_all_holdout_pairs and len(completed_holdout_rows) != len(holdout_rows):
        reasons.append(
            f"expected {len(holdout_rows)} complete hold-out pairs, found {len(completed_holdout_rows)}"
        )

    low_overlap_pairs = [
        row["pair_id"]
        for row in completed_pairs
        if int(row.get("comparison_metric_count") or 0) < minimum_metric_overlap
    ]
    if low_overlap_pairs:
        reasons.append(
            f"pairs below minimum metric overlap ({minimum_metric_overlap}): {', '.join(low_overlap_pairs)}"
        )

    low_comparable_overlap_pairs = [
        row["pair_id"]
        for row in completed_pairs
        if int(row.get("comparable_metric_count") or 0) < minimum_comparable_overlap
    ]
    if low_comparable_overlap_pairs:
        reasons.append(
            f"pairs below minimum scientifically comparable metric overlap ({minimum_comparable_overlap}): {', '.join(low_comparable_overlap_pairs)}"
        )

    sim_evidence_gaps = [
        row["pair_id"]
        for row in completed_pairs
        if int(row.get("sim_simulator_export_rows") or 0) <= 0
    ]
    if sim_evidence_gaps and bool(gates.get("require_simulator_export_rows", True)):
        reasons.append(
            f"pairs missing simulator_export rows in sim summaries: {', '.join(sim_evidence_gaps)}"
        )
    elif sim_evidence_gaps:
        warnings.append(
            f"pairs missing simulator_export rows in sim summaries: {', '.join(sim_evidence_gaps)}"
        )

    field_evidence_gaps = [
        row["pair_id"]
        for row in completed_pairs
        if int(row.get("field_field_ground_truth_rows") or 0) <= 0
    ]
    if field_evidence_gaps and bool(gates.get("require_field_ground_truth_rows", True)):
        reasons.append(
            f"pairs missing field_ground_truth rows in field summaries: {', '.join(field_evidence_gaps)}"
        )
    elif field_evidence_gaps:
        warnings.append(
            f"pairs missing field_ground_truth rows in field summaries: {', '.join(field_evidence_gaps)}"
        )

    field_sync_gaps = [
        row["pair_id"]
        for row in completed_pairs
        if str(row.get("field_scenario_id", "")).startswith("field-")
        and row.get("field_sync_status") != "configured"
    ]
    if field_sync_gaps and bool(gates.get("require_configured_field_sync", True)):
        reasons.append(
            f"pairs missing configured field sync metadata: {', '.join(field_sync_gaps)}"
        )
    elif field_sync_gaps:
        warnings.append(
            f"pairs missing configured field sync metadata: {', '.join(field_sync_gaps)}"
        )

    non_hardware_field_pairs = [
        row["pair_id"]
        for row in completed_pairs
        if str(row.get("field_scenario_id", "")).startswith("field-")
        and row.get("field_execution_mode")
        and row.get("field_execution_mode") != "real_hardware_field"
    ]
    if non_hardware_field_pairs and bool(gates.get("require_real_hardware_field_execution", True)):
        reasons.append(
            "pairs whose field side is not real_hardware_field: "
            + ", ".join(non_hardware_field_pairs)
        )
    elif non_hardware_field_pairs:
        warnings.append(
            "pairs whose field side is not real_hardware_field: "
            + ", ".join(non_hardware_field_pairs)
        )

    missing_sim_link_model_pairs = [
        row["pair_id"]
        for row in completed_pairs
        if str(row.get("sim_scenario_id", "")).startswith("sim-")
        and str(row.get("sim_link_model_export_status") or "") != "present"
    ]
    if missing_sim_link_model_pairs and bool(gates.get("require_sim_link_model_export", True)):
        reasons.append(
            "pairs missing sim link-model export: " + ", ".join(missing_sim_link_model_pairs)
        )
    elif missing_sim_link_model_pairs:
        warnings.append(
            "pairs missing sim link-model export: " + ", ".join(missing_sim_link_model_pairs)
        )

    assessment = {
        "campaign_name": config["campaign_name"],
        "readiness_profile": profile,
        "generated_at_utc": utc_now(),
        "pipeline": campaign_pipeline_metadata("campaign_assessment"),
        "quality_gates": gates,
        "ready": not reasons,
        "scale_up_recommended": not reasons if profile == "publication" else False,
        "status": (
            "ready_for_development"
            if profile == "dev" and not reasons
            else "ready_to_scale"
            if profile == "publication" and not reasons
            else "development_blocked"
            if profile == "dev"
            else "not_ready"
        ),
        "reasons": reasons or ["all configured campaign gates passed"],
        "warnings": warnings,
    }
    return assessment


def process_campaign(config: dict[str, Any], write_parquet: bool) -> Path:
    paths = campaign_paths(config)
    family_map = build_family_map(config)
    campaign_output_dir = paths["output_root"] / config["campaign_name"]
    comparisons_dir = campaign_output_dir / "comparisons"
    campaign_output_dir.mkdir(parents=True, exist_ok=True)
    comparisons_dir.mkdir(parents=True, exist_ok=True)

    inventory_rows: list[dict[str, Any]] = []

    for run in config.get("runs", []):
        family = family_map[str(run["family_id"])]
        field_scenario_id = str(run.get("field_scenario_id") or family["field_scenario_id"])
        sim_scenario_id = str(run.get("sim_scenario_id") or family["sim_scenario_id"])
        field_run_id = run.get("field_run_id")
        sim_run_id = run.get("sim_run_id")

        row: dict[str, Any] = {
            "pair_id": run["pair_id"],
            "split": run["split"],
            "family_id": run["family_id"],
            "repetition": run["repetition"],
            "field_scenario_id": field_scenario_id,
            "field_run_id": field_run_id or "",
            "sim_scenario_id": sim_scenario_id,
            "sim_run_id": sim_run_id or "",
            "status": "pending",
            "field_raw_dir": "",
            "sim_raw_dir": "",
            "field_has_gcs_logs": False,
            "field_has_rpi_logs": False,
            "field_has_ns3_exports": False,
            "field_has_ns3_flow_monitor": False,
            "field_has_ns3_link_model": False,
            "sim_has_gcs_logs": False,
            "sim_has_rpi_logs": False,
            "sim_has_ns3_exports": False,
            "sim_has_ns3_flow_monitor": False,
            "sim_has_ns3_link_model": False,
            "field_row_count": "",
            "sim_row_count": "",
            "field_sources": "",
            "sim_sources": "",
            "field_field_ground_truth_rows": "",
            "field_ui_only_rows": "",
            "sim_simulator_export_rows": "",
            "field_execution_mode": "",
            "field_execution_source": "",
            "field_execution_issues": "",
            "sim_execution_mode": "",
            "sim_execution_source": "",
            "sim_execution_issues": "",
            "sim_link_model_export_status": "",
            "sim_link_model_export_issues": "",
            "field_freshness_status": "",
            "sim_freshness_status": "",
            "comparison_metric_count": "",
            "comparison_metrics": "",
            "comparable_metric_count": "",
            "comparable_metrics": "",
            "field_sync_status": "",
            "field_sync_method": "",
            "field_sync_issues": "",
            "mean_abs_error": "",
            "mean_abs_pct_error": "",
            "error": "",
        }

        if is_pending_value(field_run_id) or is_pending_value(sim_run_id):
            inventory_rows.append(row)
            continue

        field_raw_dir = Path(run.get("field_raw_run_dir", paths["raw_root"] / field_scenario_id / str(field_run_id)))
        sim_raw_dir = Path(run.get("sim_raw_run_dir", paths["raw_root"] / sim_scenario_id / str(sim_run_id)))
        row["field_raw_dir"] = str(field_raw_dir)
        row["sim_raw_dir"] = str(sim_raw_dir)

        if not field_raw_dir.exists() or not sim_raw_dir.exists():
            row["status"] = "missing_raw"
            if not field_raw_dir.exists():
                row["error"] = f"missing field raw dir: {field_raw_dir}"
            if not sim_raw_dir.exists():
                row["error"] = (
                    f"{row['error']}; missing sim raw dir: {sim_raw_dir}".strip("; ")
                    if row["error"]
                    else f"missing sim raw dir: {sim_raw_dir}"
                )
            inventory_rows.append(row)
            continue

        try:
            field_presence = event_file_presence(field_raw_dir)
            sim_presence = event_file_presence(sim_raw_dir)
            row.update(
                {
                    "field_has_gcs_logs": field_presence["has_gcs_logs"],
                    "field_has_rpi_logs": field_presence["has_rpi_logs"],
                    "field_has_ns3_exports": field_presence["has_ns3_exports"],
                    "field_has_ns3_flow_monitor": field_presence["has_ns3_flow_monitor"],
                    "field_has_ns3_link_model": field_presence["has_ns3_link_model"],
                    "sim_has_gcs_logs": sim_presence["has_gcs_logs"],
                    "sim_has_rpi_logs": sim_presence["has_rpi_logs"],
                    "sim_has_ns3_exports": sim_presence["has_ns3_exports"],
                    "sim_has_ns3_flow_monitor": sim_presence["has_ns3_flow_monitor"],
                    "sim_has_ns3_link_model": sim_presence["has_ns3_link_model"],
                }
            )

            field_normalized_dir = normalize_raw_run(field_raw_dir, paths["normalized_root"], write_parquet)
            sim_normalized_dir = normalize_raw_run(sim_raw_dir, paths["normalized_root"], write_parquet)
            field_analysis_dir = summarize_normalized_run(field_normalized_dir, paths["analysis_root"])
            sim_analysis_dir = summarize_normalized_run(sim_normalized_dir, paths["analysis_root"])
            comparison_dir = compare_field_and_sim(
                field_analysis_dir / "run_summary.json",
                sim_analysis_dir / "run_summary.json",
                output_dir=comparisons_dir / str(run["pair_id"]),
            )

            field_summary = read_json(field_analysis_dir / "run_summary.json")
            sim_summary = read_json(sim_analysis_dir / "run_summary.json")
            comparison_summary = read_json(comparison_dir / "comparison_summary.json")

            row.update(
                {
                    "status": "complete",
                    "field_row_count": field_summary.get("row_count", ""),
                    "sim_row_count": sim_summary.get("row_count", ""),
                    "field_sources": ",".join(sorted(field_summary.get("source_counts", {}).keys())),
                    "sim_sources": ",".join(sorted(sim_summary.get("source_counts", {}).keys())),
                    "field_field_ground_truth_rows": field_summary.get("notes", {}).get(
                        "field_ground_truth_rows", ""
                    ),
                    "field_ui_only_rows": field_summary.get("notes", {}).get("ui_visualization_rows", ""),
                    "sim_simulator_export_rows": sim_summary.get("notes", {}).get(
                        "simulator_export_rows", ""
                    ),
                    "field_execution_mode": field_summary.get("execution", {}).get("mode", "") or "",
                    "field_execution_source": field_summary.get("execution", {}).get("source", "") or "",
                    "field_execution_issues": ",".join(field_summary.get("execution", {}).get("issues", [])),
                    "sim_execution_mode": sim_summary.get("execution", {}).get("mode", "") or "",
                    "sim_execution_source": sim_summary.get("execution", {}).get("source", "") or "",
                    "sim_execution_issues": ",".join(sim_summary.get("execution", {}).get("issues", [])),
                    "sim_link_model_export_status": (
                        "present"
                        if sim_summary.get("sim_export_readiness", {}).get("has_link_model_csv")
                        else "missing"
                    ),
                    "sim_link_model_export_issues": ",".join(
                        sim_summary.get("sim_export_readiness", {}).get("issues", [])
                    ),
                    "field_freshness_status": field_summary.get("freshness", {}).get("status", ""),
                    "sim_freshness_status": sim_summary.get("freshness", {}).get("status", ""),
                    "comparison_metric_count": len(comparison_summary.get("metric_rows", [])),
                    "comparison_metrics": ",".join(
                        row_data["metric"] for row_data in comparison_summary.get("metric_rows", [])
                    ),
                    "comparable_metric_count": comparison_summary.get("comparable_metric_count", 0),
                    "comparable_metrics": ",".join(
                        row_data["metric"]
                        for row_data in comparison_summary.get("comparable_metric_rows", [])
                    ),
                    "field_sync_status": field_summary.get("sync", {}).get("status", ""),
                    "field_sync_method": field_summary.get("sync", {}).get("effective_method", "") or "",
                    "field_sync_issues": ",".join(field_summary.get("sync", {}).get("issues", [])),
                    "mean_abs_error": comparison_summary.get("mean_abs_error", ""),
                    "mean_abs_pct_error": comparison_summary.get("mean_abs_pct_error", ""),
                }
            )
        except Exception as exc:  # pragma: no cover - integration path
            row["status"] = "failed"
            row["error"] = str(exc)

        inventory_rows.append(row)

    inventory_header = [
        "pair_id",
        "split",
        "family_id",
        "repetition",
        "status",
        "field_scenario_id",
        "field_run_id",
        "sim_scenario_id",
        "sim_run_id",
        "field_raw_dir",
        "sim_raw_dir",
        "field_has_gcs_logs",
        "field_has_rpi_logs",
        "field_has_ns3_exports",
        "field_has_ns3_flow_monitor",
        "field_has_ns3_link_model",
        "sim_has_gcs_logs",
        "sim_has_rpi_logs",
        "sim_has_ns3_exports",
        "sim_has_ns3_flow_monitor",
        "sim_has_ns3_link_model",
        "field_row_count",
        "sim_row_count",
        "field_sources",
        "sim_sources",
        "field_field_ground_truth_rows",
        "field_ui_only_rows",
        "sim_simulator_export_rows",
        "field_execution_mode",
        "field_execution_source",
        "field_execution_issues",
        "sim_execution_mode",
        "sim_execution_source",
        "sim_execution_issues",
        "sim_link_model_export_status",
        "sim_link_model_export_issues",
        "field_freshness_status",
        "sim_freshness_status",
        "comparison_metric_count",
        "comparison_metrics",
        "comparable_metric_count",
        "comparable_metrics",
        "field_sync_status",
        "field_sync_method",
        "field_sync_issues",
        "mean_abs_error",
        "mean_abs_pct_error",
        "error",
    ]
    write_csv(campaign_output_dir / "pair_inventory.csv", inventory_rows, inventory_header)

    table_columns = [
        ("pair_id", "Pair"),
        ("family_id", "Family"),
        ("repetition", "Rep"),
        ("status", "Status"),
        ("comparison_metric_count", "Metric Overlap"),
        ("comparable_metric_count", "Comparable Overlap"),
        ("mean_abs_error", "Mean Abs Error"),
        ("mean_abs_pct_error", "Mean Abs % Error"),
    ]
    calibration_rows = [row for row in inventory_rows if row["split"] == "calibration"]
    holdout_rows = [row for row in inventory_rows if row["split"] == "holdout"]
    write_csv(
        campaign_output_dir / "calibration_table.csv",
        calibration_rows,
        [key for key, _ in table_columns],
    )
    write_csv(
        campaign_output_dir / "holdout_validation_table.csv",
        holdout_rows,
        [key for key, _ in table_columns],
    )
    (campaign_output_dir / "calibration_table.md").write_text(
        make_markdown_table(calibration_rows, table_columns), encoding="utf-8"
    )
    (campaign_output_dir / "holdout_validation_table.md").write_text(
        make_markdown_table(holdout_rows, table_columns), encoding="utf-8"
    )

    data_quality_report = {
        "campaign_name": config["campaign_name"],
        "generated_at_utc": utc_now(),
        "config_path": config["_config_path"],
        "default_readiness_profile": config.get("default_readiness_profile", "publication"),
        "pair_counts": {
            "total": len(inventory_rows),
            "complete": sum(1 for row in inventory_rows if row["status"] == "complete"),
            "pending": sum(1 for row in inventory_rows if row["status"] == "pending"),
            "missing_raw": sum(1 for row in inventory_rows if row["status"] == "missing_raw"),
            "failed": sum(1 for row in inventory_rows if row["status"] == "failed"),
        },
        "artifact_coverage": {
            "field_pairs_with_gcs_logs": sum(1 for row in inventory_rows if row["field_has_gcs_logs"]),
            "field_pairs_with_rpi_logs": sum(1 for row in inventory_rows if row["field_has_rpi_logs"]),
            "sim_pairs_with_ns3_exports": sum(1 for row in inventory_rows if row["sim_has_ns3_exports"]),
            "sim_pairs_with_link_model_export": sum(
                1 for row in inventory_rows if row["sim_link_model_export_status"] == "present"
            ),
        },
        "issues": {
            "pending_pairs": [row["pair_id"] for row in inventory_rows if row["status"] == "pending"],
            "missing_raw_pairs": [
                row["pair_id"] for row in inventory_rows if row["status"] == "missing_raw"
            ],
            "failed_pairs": [row["pair_id"] for row in inventory_rows if row["status"] == "failed"],
            "pairs_without_metric_overlap": [
                row["pair_id"]
                for row in inventory_rows
                if row["status"] == "complete" and int(row.get("comparison_metric_count") or 0) == 0
            ],
            "pairs_without_comparable_metric_overlap": [
                row["pair_id"]
                for row in inventory_rows
                if row["status"] == "complete" and int(row.get("comparable_metric_count") or 0) == 0
            ],
            "pairs_with_field_sync_gaps": [
                row["pair_id"]
                for row in inventory_rows
                if row["status"] == "complete"
                and str(row.get("field_scenario_id", "")).startswith("field-")
                and row.get("field_sync_status") != "configured"
            ],
            "pairs_with_non_hardware_field_execution": [
                row["pair_id"]
                for row in inventory_rows
                if row["status"] == "complete"
                and str(row.get("field_scenario_id", "")).startswith("field-")
                and row.get("field_execution_mode")
                and row.get("field_execution_mode") != "real_hardware_field"
            ],
            "pairs_missing_sim_link_model_export": [
                row["pair_id"]
                for row in inventory_rows
                if row["status"] == "complete"
                and str(row.get("sim_scenario_id", "")).startswith("sim-")
                and row.get("sim_link_model_export_status") != "present"
            ],
            "pairs_with_stale_derived_outputs": [
                row["pair_id"]
                for row in inventory_rows
                if row["status"] == "complete"
                and (
                    row.get("field_freshness_status") != "current"
                    or row.get("sim_freshness_status") != "current"
                )
            ],
        },
    }
    development_readiness = readiness_assessment_from_inventory(config, inventory_rows, "dev")
    publication_readiness = readiness_assessment_from_inventory(config, inventory_rows, "publication")
    data_quality_report["readiness"] = {
        "development": development_readiness,
        "publication": publication_readiness,
    }
    write_json(campaign_output_dir / "data_quality_report.json", data_quality_report)
    data_quality_markdown = "\n".join(
        [
            "# Data Quality Report",
            "",
            f"- Campaign: `{config['campaign_name']}`",
            f"- Total pairs: `{data_quality_report['pair_counts']['total']}`",
            f"- Complete pairs: `{data_quality_report['pair_counts']['complete']}`",
            f"- Pending pairs: `{data_quality_report['pair_counts']['pending']}`",
            f"- Missing raw pairs: `{data_quality_report['pair_counts']['missing_raw']}`",
            f"- Failed pairs: `{data_quality_report['pair_counts']['failed']}`",
            "",
            "## Artifact Coverage",
            "",
            f"- field pairs with GCS logs: `{data_quality_report['artifact_coverage']['field_pairs_with_gcs_logs']}`",
            f"- field pairs with RPi logs: `{data_quality_report['artifact_coverage']['field_pairs_with_rpi_logs']}`",
            f"- sim pairs with ns-3 exports: `{data_quality_report['artifact_coverage']['sim_pairs_with_ns3_exports']}`",
            f"- sim pairs with link-model export: `{data_quality_report['artifact_coverage']['sim_pairs_with_link_model_export']}`",
            "",
            "## Readiness",
            "",
            f"- development status: `{development_readiness['status']}`",
            f"- publication status: `{publication_readiness['status']}`",
            "",
            "## Outstanding Issues",
            "",
            f"- pending pairs: `{', '.join(data_quality_report['issues']['pending_pairs']) or 'none'}`",
            f"- missing raw pairs: `{', '.join(data_quality_report['issues']['missing_raw_pairs']) or 'none'}`",
            f"- failed pairs: `{', '.join(data_quality_report['issues']['failed_pairs']) or 'none'}`",
            f"- pairs without metric overlap: `{', '.join(data_quality_report['issues']['pairs_without_metric_overlap']) or 'none'}`",
            f"- pairs without scientifically comparable metric overlap: `{', '.join(data_quality_report['issues']['pairs_without_comparable_metric_overlap']) or 'none'}`",
            f"- pairs with field sync metadata gaps: `{', '.join(data_quality_report['issues']['pairs_with_field_sync_gaps']) or 'none'}`",
            f"- pairs with non-hardware field execution: `{', '.join(data_quality_report['issues']['pairs_with_non_hardware_field_execution']) or 'none'}`",
            f"- pairs missing sim link-model export: `{', '.join(data_quality_report['issues']['pairs_missing_sim_link_model_export']) or 'none'}`",
            f"- pairs with stale derived outputs: `{', '.join(data_quality_report['issues']['pairs_with_stale_derived_outputs']) or 'none'}`",
            "",
        ]
    )
    (campaign_output_dir / "data_quality_report.md").write_text(data_quality_markdown, encoding="utf-8")

    write_json(campaign_output_dir / "development_readiness.json", development_readiness)
    write_json(campaign_output_dir / "publication_readiness.json", publication_readiness)

    def readiness_markdown(assessment: dict[str, Any], title: str) -> str:
        return "\n".join(
            [
                f"# {title}",
                "",
                f"- Campaign: `{assessment['campaign_name']}`",
                f"- Profile: `{assessment['readiness_profile']}`",
                f"- Status: `{assessment['status']}`",
                f"- Ready: `{str(assessment['ready']).lower()}`",
                "",
                "## Blocking Reasons",
                "",
                *[f"- {reason}" for reason in assessment["reasons"]],
                "",
                "## Warnings",
                "",
                *([f"- {warning}" for warning in assessment.get("warnings", [])] or ["- none"]),
                "",
            ]
        )

    (campaign_output_dir / "development_readiness.md").write_text(
        readiness_markdown(development_readiness, "Development Readiness"),
        encoding="utf-8",
    )
    (campaign_output_dir / "publication_readiness.md").write_text(
        readiness_markdown(publication_readiness, "Publication Readiness"),
        encoding="utf-8",
    )

    recommendation = publication_readiness
    write_json(campaign_output_dir / "scale_up_recommendation.json", recommendation)
    recommendation_markdown = "\n".join(
        [
            "# Scale-Up Recommendation",
            "",
            f"- Campaign: `{recommendation['campaign_name']}`",
            f"- Status: `{recommendation['status']}`",
            f"- Scale up recommended: `{str(recommendation['scale_up_recommended']).lower()}`",
            "",
            "## Reasons",
            "",
            *[f"- {reason}" for reason in recommendation["reasons"]],
            "",
            "## Warnings",
            "",
            *([f"- {warning}" for warning in recommendation.get("warnings", [])] or ["- none"]),
            "",
        ]
    )
    (campaign_output_dir / "scale_up_recommendation.md").write_text(
        recommendation_markdown, encoding="utf-8"
    )

    return campaign_output_dir


def main() -> int:
    args = parse_args()
    config = load_campaign_config(args.campaign_config)
    output_dir = process_campaign(config, args.write_parquet)
    print(output_dir)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
