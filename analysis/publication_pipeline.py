#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import json
import math
import statistics
import sys
from collections import Counter, defaultdict
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

CANONICAL_COLUMNS = [
    "scenario_id",
    "run_id",
    "event_id",
    "source",
    "event_type",
    "uav_id",
    "timestamp_utc",
    "timestamp_monotonic_ms",
    "rat",
    "security_profile",
    "sequence_id",
    "command_id",
    "latitude",
    "longitude",
    "altitude_m",
    "speed_mps",
    "heading_deg",
    "battery_pct",
    "battery_voltage_v",
    "rtt_ms",
    "delay_ms",
    "jitter_ms",
    "packet_loss_pct",
    "throughput_mbps",
    "rssi_dbm",
    "rsrp_dbm",
    "rsrq_db",
    "sinr_db",
    "status",
    "note",
]

EXTENDED_COLUMNS = [
    "evidence_layer",
    "metric_origin",
    "command_name",
    "command_code",
    "message_name",
    "sim_time_s",
    "sync_method",
    "sync_offset_ms",
    "battery_current_ma",
    "flow_id",
    "flow_type",
    "pdr",
    "channel",
    "result_code",
    "measurement_family",
    "link_quality",
    "serving_label",
    "raw_source_file",
]

ALL_COLUMNS = CANONICAL_COLUMNS + EXTENDED_COLUMNS
NUMERIC_FIELDS = {
    "timestamp_monotonic_ms",
    "latitude",
    "longitude",
    "altitude_m",
    "speed_mps",
    "heading_deg",
    "battery_pct",
    "battery_voltage_v",
    "rtt_ms",
    "delay_ms",
    "jitter_ms",
    "packet_loss_pct",
    "throughput_mbps",
    "rssi_dbm",
    "rsrp_dbm",
    "rsrq_db",
    "sinr_db",
    "sim_time_s",
    "sync_offset_ms",
    "battery_current_ma",
    "flow_id",
    "pdr",
    "result_code",
    "command_code",
}
ALIAS_MAP = {
    "battery_percentage": "battery_pct",
    "battery_voltage": "battery_voltage_v",
    "battery_current_milliamps": "battery_current_ma",
}
EVIDENCE_ORDER = ["field_ground_truth", "simulator_export", "ui_visualization_only", "derived_postprocess"]
COMPARISON_METRICS = [
    "mean_rtt_ms",
    "p95_rtt_ms",
    "mean_jitter_ms",
    "mean_packet_loss_pct",
    "mean_throughput_mbps",
    "mean_rsrp_dbm",
    "mean_rsrq_db",
    "mean_sinr_db",
]


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="milliseconds").replace("+00:00", "Z")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Normalize publication-grade raw logs and derive run summaries/comparisons."
    )
    subparsers = parser.add_subparsers(dest="command", required=True)

    normalize = subparsers.add_parser("normalize", help="Normalize one raw run directory into merged CSV.")
    normalize.add_argument("--raw-run-dir", required=True, type=Path)
    normalize.add_argument("--normalized-root", type=Path, default=Path("logs/normalized"))
    normalize.add_argument("--write-parquet", action="store_true")

    summarize = subparsers.add_parser("summarize", help="Create per-run summaries from normalized CSV.")
    summarize.add_argument("--normalized-run-dir", required=True, type=Path)
    summarize.add_argument("--analysis-root", type=Path, default=Path("logs/analysis"))

    compare = subparsers.add_parser("compare", help="Compare one field summary against one sim summary.")
    compare.add_argument("--field-summary", required=True, type=Path)
    compare.add_argument("--sim-summary", required=True, type=Path)
    compare.add_argument("--output-dir", type=Path)

    return parser.parse_args()


def read_json(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def load_rows_from_csv(path: Path) -> list[dict[str, str]]:
    with path.open("r", encoding="utf-8", newline="") as handle:
        return list(csv.DictReader(handle))


def write_csv(path: Path, rows: list[dict[str, Any]], header: list[str]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=header)
        writer.writeheader()
        for row in rows:
            writer.writerow({column: row.get(column, "") for column in header})


def write_json(path: Path, payload: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, indent=2, sort_keys=False), encoding="utf-8")


def coerce_number(value: Any) -> float | int | None:
    if value in ("", None):
        return None
    try:
        numeric = float(value)
    except (TypeError, ValueError):
        return None
    if not math.isfinite(numeric):
        return None
    if numeric.is_integer():
        return int(numeric)
    return numeric


def parse_numeric(row: dict[str, str], field: str) -> float | None:
    value = coerce_number(row.get(field))
    return float(value) if value is not None else None


def percentile(values: list[float], fraction: float) -> float | None:
    if not values:
        return None
    if len(values) == 1:
        return values[0]
    return statistics.quantiles(values, n=100, method="inclusive")[int(fraction * 100) - 1]


def mean_or_none(values: list[float]) -> float | None:
    return statistics.fmean(values) if values else None


def infer_evidence_layer(source: str, event_type: str, row: dict[str, Any]) -> str:
    if row.get("evidence_layer"):
        return str(row["evidence_layer"])
    if row.get("metric_origin") == "flow_monitor":
        return "simulator_export"
    if event_type == "sim_snapshot":
        return "ui_visualization_only"
    if source.startswith("ns3"):
        return "simulator_export"
    if event_type in {"command_tx", "command_rx", "command_ack", "telemetry_rx", "network_sample", "packet_forward", "battery_sample", "sync_status"}:
        return "field_ground_truth"
    return ""


def infer_metric_origin(event_type: str, row: dict[str, Any]) -> str:
    if row.get("metric_origin"):
        return str(row["metric_origin"])
    if event_type == "network_sample":
        return "direct_measurement"
    if event_type in {"command_tx", "command_rx", "command_ack", "telemetry_rx", "packet_forward", "battery_sample"}:
        return "direct_observation"
    if event_type == "sync_status":
        return "runtime_metadata"
    return ""


def infer_measurement_family(row: dict[str, Any]) -> str:
    if row.get("measurement_family"):
        return str(row["measurement_family"])

    event_type = str(row.get("event_type") or "")
    metric_origin = str(row.get("metric_origin") or "")
    status = str(row.get("status") or "").lower()

    if metric_origin == "flow_monitor" or event_type == "flow_performance_sample":
        return "sim_flow_performance"
    if metric_origin == "link_model":
        return "sim_link_model"
    if event_type == "network_sample":
        if status == "measured":
            return "field_modem"
        if status == "simulated":
            return "proxy_modem_simulated"
        return "network_sample_generic"
    if event_type == "command_ack":
        return "command_ack"
    if event_type == "battery_sample":
        return "vehicle_status"
    if event_type == "sim_snapshot":
        return "snapshot_estimate"
    return ""


def scenario_parts(scenario_id: str) -> dict[str, Any]:
    tokens = [token for token in scenario_id.split("-") if token]
    parsed = {
        "domain": tokens[0] if len(tokens) > 0 else "",
        "rat": tokens[1] if len(tokens) > 1 else "",
        "uav_count": tokens[2] if len(tokens) > 2 else "",
        "motion": tokens[3] if len(tokens) > 3 else "",
        "security_profile": tokens[4] if len(tokens) > 4 else "",
        "variant": "-".join(tokens[5:]) if len(tokens) > 5 else "",
    }
    parsed["is_valid_pattern"] = len(tokens) >= 6
    return parsed


def normalize_sync_method(value: Any) -> str:
    if value is None:
        return ""
    return str(value).strip()


def is_unspecified_sync_method(value: Any) -> bool:
    method = normalize_sync_method(value)
    return not method or method.lower() == "unspecified"


def summarize_sync_metadata(
    dataset_manifest: dict[str, Any], rows: list[dict[str, str]]
) -> dict[str, Any]:
    scenario = dataset_manifest.get("scenario", {})
    manifest_timing = dataset_manifest.get("run_manifest", {}).get("timing", {})
    manifest_method = normalize_sync_method(manifest_timing.get("sync_method"))
    source_methods = {
        source: normalize_sync_method(metadata.get("sync_method"))
        for source, metadata in sorted((dataset_manifest.get("source_metadata") or {}).items())
    }
    sync_event_methods = sorted(
        {
            normalize_sync_method(row.get("sync_method"))
            for row in rows
            if row.get("event_type") == "sync_status"
            and normalize_sync_method(row.get("sync_method"))
        }
    )
    specified_methods = {
        method
        for method in [manifest_method, *source_methods.values(), *sync_event_methods]
        if not is_unspecified_sync_method(method)
    }

    issues: list[str] = []
    if scenario.get("domain") == "field":
        if is_unspecified_sync_method(manifest_method):
            issues.append("manifest_sync_method_unspecified")
        for source, method in source_methods.items():
            if is_unspecified_sync_method(method):
                issues.append(f"source_sync_method_unspecified:{source}")
        if not sync_event_methods:
            issues.append("sync_status_event_missing")
        elif any(is_unspecified_sync_method(method) for method in sync_event_methods):
            issues.append("sync_status_event_unspecified")
    if len(specified_methods) > 1:
        issues.append("inconsistent_sync_methods")

    effective_method = ""
    if not is_unspecified_sync_method(manifest_method):
        effective_method = manifest_method
    elif len(specified_methods) == 1:
        effective_method = next(iter(specified_methods))

    if "inconsistent_sync_methods" in issues:
        status = "inconsistent"
    elif issues:
        status = "unspecified"
    else:
        status = "configured" if effective_method else "unspecified"

    return {
        "status": status,
        "effective_method": effective_method or None,
        "manifest_method": manifest_method or None,
        "manifest_note": manifest_timing.get("notes") or None,
        "source_methods": source_methods,
        "sync_event_methods": sync_event_methods,
        "issues": issues,
    }


def preferred_evidence_layers_for_scenario(scenario_id: str) -> list[str]:
    domain = scenario_parts(scenario_id).get("domain")
    if domain == "field":
        return ["field_ground_truth"]
    if domain == "sim":
        return ["simulator_export"]
    return ["field_ground_truth", "simulator_export"]


def normalized_row(base: dict[str, Any]) -> dict[str, Any]:
    row = {column: "" for column in ALL_COLUMNS}
    for key, value in base.items():
        canonical = ALIAS_MAP.get(key, key)
        if canonical not in row:
            continue
        if canonical in NUMERIC_FIELDS:
            numeric = coerce_number(value)
            row[canonical] = "" if numeric is None else numeric
        else:
            row[canonical] = "" if value is None else value
    row["evidence_layer"] = infer_evidence_layer(str(row["source"]), str(row["event_type"]), row)
    row["metric_origin"] = infer_metric_origin(str(row["event_type"]), row)
    row["measurement_family"] = infer_measurement_family(row)
    return row


def preferred_measurement_families_for_metric(scenario_id: str, metric_name: str) -> set[str] | None:
    domain = scenario_parts(scenario_id).get("domain")
    if domain == "field":
        if metric_name in {"mean_rtt_ms", "p95_rtt_ms", "mean_rsrp_dbm", "mean_rsrq_db", "mean_sinr_db"}:
            return {"field_modem"}
        if metric_name in {"mean_jitter_ms", "mean_packet_loss_pct", "mean_throughput_mbps"}:
            return {"field_modem"}
        return None
    if domain == "sim":
        if metric_name in {"mean_rtt_ms", "p95_rtt_ms", "mean_rsrp_dbm", "mean_rsrq_db", "mean_sinr_db"}:
            return {"sim_link_model"}
        if metric_name in {"mean_jitter_ms", "mean_packet_loss_pct", "mean_throughput_mbps"}:
            return {"sim_flow_performance"}
        return None
    return None


def normalize_raw_run(raw_run_dir: Path, normalized_root: Path, write_parquet: bool) -> Path:
    if not raw_run_dir.is_dir():
        raise FileNotFoundError(f"raw run directory not found: {raw_run_dir}")

    scenario_id = raw_run_dir.parent.name
    run_id = raw_run_dir.name
    normalized_dir = normalized_root / scenario_id / run_id
    rows: list[dict[str, Any]] = []
    source_metadata: dict[str, Any] = {}

    for path in sorted(raw_run_dir.glob("*_events.jsonl")):
        source_name = path.stem.removesuffix("_events")
        with path.open("r", encoding="utf-8") as handle:
            for line_number, line in enumerate(handle, start=1):
                line = line.strip()
                if not line:
                    continue
                payload = json.loads(line)
                payload.setdefault("scenario_id", scenario_id)
                payload.setdefault("run_id", run_id)
                payload.setdefault("source", source_name)
                payload["raw_source_file"] = path.name
                rows.append(normalized_row(payload))

    for path in sorted(raw_run_dir.glob("*_metadata.json")):
        source_metadata[path.stem.removesuffix("_metadata")] = read_json(path)

    for path in sorted(raw_run_dir.glob("ns3_*_flow_monitor.csv")):
        for index, payload in enumerate(load_rows_from_csv(path), start=1):
            packet_loss = payload.get("packet_loss_pct")
            if not packet_loss and payload.get("tx_packets") and payload.get("lost_packets"):
                tx_packets = coerce_number(payload["tx_packets"]) or 0
                lost_packets = coerce_number(payload["lost_packets"]) or 0
                packet_loss = (float(lost_packets) / float(tx_packets) * 100.0) if tx_packets else ""
            rows.append(
                normalized_row(
                    {
                        "scenario_id": payload.get("scenario_id", scenario_id),
                        "run_id": payload.get("run_id", run_id),
                        "event_id": f"{payload.get('source', path.stem)}-csv-{index:06d}",
                        "source": payload.get("source", path.stem),
                        "event_type": "flow_performance_sample",
                        "rat": payload.get("rat"),
                        "security_profile": payload.get("security_profile"),
                        "delay_ms": payload.get("mean_delay_ms"),
                        "jitter_ms": payload.get("mean_jitter_ms"),
                        "packet_loss_pct": packet_loss,
                        "throughput_mbps": payload.get("throughput_mbps"),
                        "status": "exported",
                        "note": f"flow_monitor:{payload.get('flow_type', 'unknown')}",
                        "evidence_layer": payload.get("evidence_layer", "simulator_export"),
                        "metric_origin": payload.get("metric_origin", "flow_monitor"),
                        "measurement_family": payload.get("measurement_family", "sim_flow_performance"),
                        "sim_time_s": payload.get("sim_time_s"),
                        "flow_id": payload.get("flow_id"),
                        "flow_type": payload.get("flow_type"),
                        "pdr": payload.get("pdr"),
                        "raw_source_file": path.name,
                    }
                )
            )

    for path in sorted(raw_run_dir.glob("ns3_*_link_model.csv")):
        for index, payload in enumerate(load_rows_from_csv(path), start=1):
            rows.append(
                normalized_row(
                    {
                        "scenario_id": payload.get("scenario_id", scenario_id),
                        "run_id": payload.get("run_id", run_id),
                        "event_id": f"{payload.get('source', path.stem)}-csv-{index:06d}",
                        "source": payload.get("source", path.stem),
                        "event_type": "network_sample",
                        "uav_id": payload.get("uav_id"),
                        "rat": payload.get("rat"),
                        "security_profile": payload.get("security_profile"),
                        "rtt_ms": payload.get("ping_ms"),
                        "jitter_ms": payload.get("jitter_ms"),
                        "packet_loss_pct": payload.get("packet_loss_pct"),
                        "throughput_mbps": payload.get("throughput_mbps"),
                        "rssi_dbm": payload.get("rssi_dbm"),
                        "rsrp_dbm": payload.get("rsrp_dbm"),
                        "rsrq_db": payload.get("rsrq_db"),
                        "sinr_db": payload.get("sinr_db"),
                        "status": "exported",
                        "note": f"link_model:{payload.get('quality', 'unknown')}",
                        "link_quality": payload.get("quality"),
                        "serving_label": payload.get("serving_label"),
                        "evidence_layer": payload.get("evidence_layer", "simulator_export"),
                        "metric_origin": payload.get("metric_origin", "link_model"),
                        "measurement_family": payload.get("measurement_family", "sim_link_model"),
                        "sim_time_s": payload.get("sim_time_s"),
                        "raw_source_file": path.name,
                    }
                )
            )

    normalized_dir.mkdir(parents=True, exist_ok=True)
    merged_csv = normalized_dir / "events.csv"
    write_csv(merged_csv, rows, ALL_COLUMNS)

    parquet_status = ""
    if write_parquet:
        try:
            import pandas as pd  # type: ignore
            import pyarrow  # noqa: F401

            pd.DataFrame(rows).to_parquet(normalized_dir / "events.parquet", index=False)
            parquet_status = "written"
        except Exception as exc:  # pragma: no cover - verification depends on environment
            parquet_status = f"skipped: {exc}"

    run_manifest_path = raw_run_dir / "run_manifest.json"
    dataset_manifest = {
        "schema_name": "networkplanner_normalized_dataset",
        "schema_version": 1,
        "generated_at_utc": utc_now(),
        "scenario_id": scenario_id,
        "run_id": run_id,
        "raw_run_dir": str(raw_run_dir.resolve()),
        "normalized_dir": str(normalized_dir.resolve()),
        "row_count": len(rows),
        "scenario": scenario_parts(scenario_id),
        "sources": sorted({row["source"] for row in rows if row["source"]}),
        "evidence_layers": [layer for layer in EVIDENCE_ORDER if layer in {row["evidence_layer"] for row in rows}],
        "source_metadata": source_metadata,
        "run_manifest": read_json(run_manifest_path) if run_manifest_path.exists() else {},
        "parquet_status": parquet_status or "not_requested",
    }
    write_json(normalized_dir / "dataset_manifest.json", dataset_manifest)
    return normalized_dir


def summarize_normalized_run(normalized_run_dir: Path, analysis_root: Path) -> Path:
    dataset_manifest = read_json(normalized_run_dir / "dataset_manifest.json")
    rows = load_rows_from_csv(normalized_run_dir / "events.csv")
    scenario_id = dataset_manifest["scenario_id"]
    run_id = dataset_manifest["run_id"]
    analysis_dir = analysis_root / scenario_id / run_id
    analysis_dir.mkdir(parents=True, exist_ok=True)

    event_counts = Counter(row["event_type"] for row in rows if row["event_type"])
    source_counts = Counter(row["source"] for row in rows if row["source"])
    evidence_counts = Counter(row["evidence_layer"] for row in rows if row["evidence_layer"])

    ack_rows = [row for row in rows if row["event_type"] == "command_ack" and parse_numeric(row, "rtt_ms") is not None]
    ack_rtts = [parse_numeric(row, "rtt_ms") for row in ack_rows]
    ack_rtts = [value for value in ack_rtts if value is not None]
    accepted_count = sum(1 for row in ack_rows if row.get("status") == "accepted")
    command_summary = {
        "count": len(ack_rtts),
        "accepted_count": accepted_count,
        "success_rate": (accepted_count / len(ack_rows)) if ack_rows else None,
        "mean_rtt_ms": mean_or_none(ack_rtts),
        "median_rtt_ms": statistics.median(ack_rtts) if ack_rtts else None,
        "p95_rtt_ms": percentile(ack_rtts, 0.95),
        "max_rtt_ms": max(ack_rtts) if ack_rtts else None,
    }

    telemetry_groups: dict[tuple[str, str], list[tuple[float, int | None]]] = defaultdict(list)
    for row in rows:
        if row["event_type"] not in {"telemetry_rx", "battery_sample"}:
            continue
        timestamp = parse_numeric(row, "timestamp_monotonic_ms")
        if timestamp is None:
            continue
        sequence_value = coerce_number(row.get("sequence_id"))
        sequence = int(sequence_value) if sequence_value is not None else None
        telemetry_groups[(row.get("uav_id", "") or "unknown", row.get("message_name", "") or row["event_type"])].append((timestamp, sequence))

    telemetry_rows = []
    for (uav_id, message_name), samples in sorted(telemetry_groups.items()):
        samples.sort(key=lambda item: item[0])
        inter_arrivals = [samples[index][0] - samples[index - 1][0] for index in range(1, len(samples))]
        sequence_gaps = 0
        for index in range(1, len(samples)):
            previous = samples[index - 1][1]
            current = samples[index][1]
            if previous is not None and current is not None and current > previous + 1:
                sequence_gaps += current - previous - 1
        telemetry_rows.append(
            {
                "uav_id": uav_id,
                "message_name": message_name,
                "sample_count": len(samples),
                "mean_inter_arrival_ms": mean_or_none(inter_arrivals),
                "p95_inter_arrival_ms": percentile(inter_arrivals, 0.95),
                "sequence_gap_count": sequence_gaps,
            }
        )

    metric_summary_rows = []
    representative_metrics: dict[str, float | None] = {}
    representative_metric_details: dict[str, dict[str, Any]] = {}
    preferred_evidence_layers = preferred_evidence_layers_for_scenario(scenario_id)
    sync_summary = summarize_sync_metadata(dataset_manifest, rows)
    evidence_rows = [
        row
        for row in rows
        if row["evidence_layer"] in preferred_evidence_layers
    ]
    metric_map = {
        "mean_rtt_ms": ("rtt_ms", {"network_sample"}),
        "p95_rtt_ms": ("rtt_ms", {"network_sample"}),
        "mean_jitter_ms": ("jitter_ms", {"network_sample", "flow_performance_sample"}),
        "mean_packet_loss_pct": ("packet_loss_pct", {"network_sample", "flow_performance_sample"}),
        "mean_throughput_mbps": ("throughput_mbps", {"network_sample", "flow_performance_sample"}),
        "mean_rsrp_dbm": ("rsrp_dbm", {"network_sample"}),
        "mean_rsrq_db": ("rsrq_db", {"network_sample"}),
        "mean_sinr_db": ("sinr_db", {"network_sample"}),
    }
    for metric_name, (field_name, preferred_event_types) in metric_map.items():
        preferred_families = preferred_measurement_families_for_metric(scenario_id, metric_name)
        selected_rows = [
            row
            for row in evidence_rows
            if row["event_type"] in preferred_event_types
            and (
                preferred_families is None
                or str(row.get("measurement_family") or "") in preferred_families
            )
        ]
        numeric_values = [
            value
            for value in (parse_numeric(row, field_name) for row in selected_rows)
            if value is not None
        ]
        fallback_used = ""
        if not numeric_values and field_name == "rtt_ms":
            selected_rows = [row for row in ack_rows]
            numeric_values = [
                value
                for value in (parse_numeric(row, field_name) for row in selected_rows)
                if value is not None
            ]
            if numeric_values:
                fallback_used = "command_ack"
        if not numeric_values and field_name == "packet_loss_pct":
            selected_rows = [
                row
                for row in evidence_rows
                if row["event_type"] == "battery_sample"
            ]
            numeric_values = [
                value
                for value in (parse_numeric(row, field_name) for row in selected_rows)
                if value is not None
            ]
            if numeric_values:
                fallback_used = "battery_sample"
        representative_metrics[metric_name] = (
            percentile(numeric_values, 0.95) if metric_name.startswith("p95_") else mean_or_none(numeric_values)
        )
        representative_metric_details[metric_name] = {
            "field_name": field_name,
            "source_event_types": sorted({str(row.get("event_type") or "") for row in selected_rows if row.get("event_type")}),
            "source_metric_origins": sorted({str(row.get("metric_origin") or "") for row in selected_rows if row.get("metric_origin")}),
            "measurement_families": sorted({str(row.get("measurement_family") or "") for row in selected_rows if row.get("measurement_family")}),
            "fallback_used": fallback_used or None,
        }

    for field_name in ["rtt_ms", "delay_ms", "jitter_ms", "packet_loss_pct", "throughput_mbps", "rsrp_dbm", "rsrq_db", "sinr_db"]:
        values = [parse_numeric(row, field_name) for row in evidence_rows]
        numeric_values = [value for value in values if value is not None]
        metric_summary_rows.append(
            {
                "metric": field_name,
                "count": len(numeric_values),
                "mean": mean_or_none(numeric_values),
                "median": statistics.median(numeric_values) if numeric_values else None,
                "p95": percentile(numeric_values, 0.95),
            }
        )

    summary = {
        "schema_name": "networkplanner_run_summary",
        "schema_version": 1,
        "generated_at_utc": utc_now(),
        "scenario_id": scenario_id,
        "run_id": run_id,
        "scenario": dataset_manifest["scenario"],
        "row_count": len(rows),
        "event_counts": dict(event_counts),
        "source_counts": dict(source_counts),
        "evidence_layer_counts": dict(evidence_counts),
        "command_summary": command_summary,
        "telemetry_continuity": telemetry_rows,
        "representative_metrics": representative_metrics,
        "representative_metric_details": representative_metric_details,
        "sync": sync_summary,
        "notes": {
            "preferred_evidence_layers": preferred_evidence_layers,
            "ui_visualization_rows": evidence_counts.get("ui_visualization_only", 0),
            "simulator_export_rows": evidence_counts.get("simulator_export", 0),
            "field_ground_truth_rows": evidence_counts.get("field_ground_truth", 0),
        },
    }

    write_json(analysis_dir / "run_summary.json", summary)
    write_csv(analysis_dir / "telemetry_continuity.csv", telemetry_rows, ["uav_id", "message_name", "sample_count", "mean_inter_arrival_ms", "p95_inter_arrival_ms", "sequence_gap_count"])
    write_csv(analysis_dir / "metric_summary.csv", metric_summary_rows, ["metric", "count", "mean", "median", "p95"])
    write_csv(analysis_dir / "run_summary.csv", [{**{key: summary[key] for key in ["scenario_id", "run_id", "row_count"]}, **representative_metrics}], ["scenario_id", "run_id", "row_count", *representative_metrics.keys()])

    publication_table = [
        "| Metric | Value |",
        "| --- | --- |",
        f"| Mean RTT (ms) | {format_metric(representative_metrics['mean_rtt_ms'])} |",
        f"| P95 RTT (ms) | {format_metric(representative_metrics['p95_rtt_ms'])} |",
        f"| Mean jitter (ms) | {format_metric(representative_metrics['mean_jitter_ms'])} |",
        f"| Mean packet loss (%) | {format_metric(representative_metrics['mean_packet_loss_pct'])} |",
        f"| Mean throughput (Mbps) | {format_metric(representative_metrics['mean_throughput_mbps'])} |",
        f"| Mean RSRQ (dB) | {format_metric(representative_metrics['mean_rsrq_db'])} |",
        f"| Command ACK success rate | {format_metric(command_summary['success_rate'], scale=100.0, suffix='%')} |",
    ]
    (analysis_dir / "publication_table.md").write_text("\n".join(publication_table) + "\n", encoding="utf-8")
    return analysis_dir


def format_metric(value: Any, scale: float = 1.0, suffix: str = "") -> str:
    if value is None or value == "":
        return "n/a"
    return f"{float(value) * scale:.3f}{suffix}"


def compare_field_and_sim(field_summary_path: Path, sim_summary_path: Path, output_dir: Path | None) -> Path:
    field_summary = read_json(field_summary_path)
    sim_summary = read_json(sim_summary_path)
    if output_dir is None:
        output_dir = Path("logs/analysis/comparisons") / f"{field_summary['run_id']}__vs__{sim_summary['run_id']}"
    output_dir.mkdir(parents=True, exist_ok=True)

    rows = []
    comparable_rows = []
    abs_errors = []
    abs_pct_errors = []
    comparable_abs_errors = []
    comparable_abs_pct_errors = []

    def is_scientifically_comparable(metric: str, field_detail: dict[str, Any], sim_detail: dict[str, Any]) -> bool:
        field_families = set(field_detail.get("measurement_families") or [])
        sim_families = set(sim_detail.get("measurement_families") or [])
        if not field_families or not sim_families:
            return False
        return (
            metric in {"mean_rtt_ms", "p95_rtt_ms", "mean_rsrp_dbm", "mean_rsrq_db", "mean_sinr_db"}
            and "field_modem" in field_families
            and "sim_link_model" in sim_families
        )

    for metric in COMPARISON_METRICS:
        field_value = field_summary["representative_metrics"].get(metric)
        sim_value = sim_summary["representative_metrics"].get(metric)
        if field_value is None or sim_value is None:
            continue
        abs_error = abs(float(sim_value) - float(field_value))
        rel_error_pct = (abs_error / abs(float(field_value)) * 100.0) if float(field_value) != 0.0 else None
        abs_errors.append(abs_error)
        if rel_error_pct is not None:
            abs_pct_errors.append(rel_error_pct)
        field_detail = field_summary.get("representative_metric_details", {}).get(metric, {})
        sim_detail = sim_summary.get("representative_metric_details", {}).get(metric, {})
        comparable = is_scientifically_comparable(metric, field_detail, sim_detail)
        row = {
            "metric": metric,
            "field_value": field_value,
            "sim_value": sim_value,
            "abs_error": abs_error,
            "relative_error_pct": rel_error_pct,
            "scientifically_comparable": comparable,
            "field_measurement_families": ",".join(field_detail.get("measurement_families", [])),
            "sim_measurement_families": ",".join(sim_detail.get("measurement_families", [])),
        }
        rows.append(row)
        if comparable:
            comparable_rows.append(row)
            comparable_abs_errors.append(abs_error)
            if rel_error_pct is not None:
                comparable_abs_pct_errors.append(rel_error_pct)

    comparison = {
        "schema_name": "networkplanner_field_vs_sim_comparison",
        "schema_version": 1,
        "generated_at_utc": utc_now(),
        "field_scenario_id": field_summary["scenario_id"],
        "field_run_id": field_summary["run_id"],
        "sim_scenario_id": sim_summary["scenario_id"],
        "sim_run_id": sim_summary["run_id"],
        "scenario_match": {
            "field": field_summary.get("scenario", {}),
            "sim": sim_summary.get("scenario", {}),
            "rat_match": field_summary.get("scenario", {}).get("rat") == sim_summary.get("scenario", {}).get("rat"),
            "security_match": field_summary.get("scenario", {}).get("security_profile") == sim_summary.get("scenario", {}).get("security_profile"),
            "uav_count_match": field_summary.get("scenario", {}).get("uav_count") == sim_summary.get("scenario", {}).get("uav_count"),
        },
        "metric_rows": rows,
        "comparable_metric_rows": comparable_rows,
        "structural_metric_count": len(rows),
        "comparable_metric_count": len(comparable_rows),
        "mean_abs_error": mean_or_none(abs_errors),
        "mean_abs_pct_error": mean_or_none(abs_pct_errors),
        "comparable_mean_abs_error": mean_or_none(comparable_abs_errors),
        "comparable_mean_abs_pct_error": mean_or_none(comparable_abs_pct_errors),
    }

    write_json(output_dir / "comparison_summary.json", comparison)
    write_csv(
        output_dir / "comparison_metrics.csv",
        rows,
        [
            "metric",
            "field_value",
            "sim_value",
            "abs_error",
            "relative_error_pct",
            "scientifically_comparable",
            "field_measurement_families",
            "sim_measurement_families",
        ],
    )
    markdown = [
        "| Metric | Field | Sim | Abs. Error | Rel. Error (%) | Comparable |",
        "| --- | --- | --- | --- | --- | --- |",
    ]
    for row in rows:
        markdown.append(
            f"| {row['metric']} | {format_metric(row['field_value'])} | {format_metric(row['sim_value'])} | "
            f"{format_metric(row['abs_error'])} | {format_metric(row['relative_error_pct'])} | "
            f"{'yes' if row['scientifically_comparable'] else 'no'} |"
        )
    markdown.append("")
    markdown.append(f"Mean absolute error: {format_metric(comparison['mean_abs_error'])}")
    markdown.append(f"Mean absolute percentage error: {format_metric(comparison['mean_abs_pct_error'])}")
    markdown.append(
        f"Comparable metric count: {comparison['comparable_metric_count']}"
    )
    markdown.append(
        f"Comparable mean absolute error: {format_metric(comparison['comparable_mean_abs_error'])}"
    )
    (output_dir / "comparison_table.md").write_text("\n".join(markdown) + "\n", encoding="utf-8")
    return output_dir


def main() -> int:
    args = parse_args()
    if args.command == "normalize":
        normalized_dir = normalize_raw_run(args.raw_run_dir, args.normalized_root, args.write_parquet)
        print(normalized_dir)
        return 0
    if args.command == "summarize":
        analysis_dir = summarize_normalized_run(args.normalized_run_dir, args.analysis_root)
        print(analysis_dir)
        return 0
    if args.command == "compare":
        comparison_dir = compare_field_and_sim(args.field_summary, args.sim_summary, args.output_dir)
        print(comparison_dir)
        return 0
    raise ValueError(f"unsupported command: {args.command}")


if __name__ == "__main__":
    sys.exit(main())
