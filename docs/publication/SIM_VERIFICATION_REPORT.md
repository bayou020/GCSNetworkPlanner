# Sim Verification Report

Date: 2026-04-13

This report covers the current simulator-only publication evidence for:

- LTE
- 5G NR

It does not claim calibration or sim-to-real validation. It documents the sim-side export and analysis state after the measurement-family fixes.

## 1. Summary

The simulator path is now publication-usable for both LTE and NR.

For both RATs, one real `ns-3` run was executed and verified to produce:

- a FlowMonitor export
- a separate link-model export
- publication metadata declaring both measurement families
- normalized outputs
- per-run summaries that no longer treat FlowMonitor delay as RTT

Current status:

- LTE sim path: verified
- NR sim path: verified
- measurement-family separation: verified
- calibration readiness: not claimed

## 2. What Was Verified

The following was verified end to end for each RAT:

1. raw simulator export lands in `logs/raw/<scenario>/<run>/...`
2. FlowMonitor rows are preserved as flow-performance data
3. link-model rows are exported separately
4. normalization ingests both files
5. per-run summaries select representative metrics from the correct measurement family

Expected family split:

- `sim_link_model`
  - RTT
  - RSRP
  - RSRQ
  - SINR
- `sim_flow_performance`
  - jitter
  - packet loss
  - throughput

## 3. LTE Verification

### Command used

```bash
env \
  NP_SCENARIO_ID=sim-lte-1-static-none-linkmodelcheck \
  NP_RUN_ID=20260413T180500Z \
  NP_LOG_ROOT="$PWD/logs" \
  UAVS=1 \
  BASE_STATIONS=1 \
  SIM_TIME=5 \
  SECURITY=none \
  LIVE=0 \
  ./sim/ns3/scripts/run_uav_lte.sh
```

### Raw artifacts

Raw run directory:

- `logs/raw/sim-lte-1-static-none-linkmodelcheck/20260413T180500Z`

Produced files:

- `ns3_lte_flow_monitor.csv`
- `ns3_lte_link_model.csv`
- `ns3_lte_metadata.json`

### Normalized and analysis outputs

- `logs/normalized/sim-lte-1-static-none-linkmodelcheck/20260413T180500Z/events.csv`
- `logs/normalized/sim-lte-1-static-none-linkmodelcheck/20260413T180500Z/dataset_manifest.json`
- `logs/analysis/sim-lte-1-static-none-linkmodelcheck/20260413T180500Z/run_summary.json`
- `logs/analysis/sim-lte-1-static-none-linkmodelcheck/20260413T180500Z/metric_summary.csv`
- `logs/analysis/sim-lte-1-static-none-linkmodelcheck/20260413T180500Z/publication_table.md`

### Verified summary behavior

Representative metrics:

- `mean_rtt_ms = 29.92899175`
- `p95_rtt_ms = 30.66289655`
- `mean_jitter_ms = 0.0125`
- `mean_packet_loss_pct = 0.0`
- `mean_throughput_mbps = 0.0096145`
- `mean_rsrp_dbm = -70.45711925`
- `mean_rsrq_db = -6.0965335`
- `mean_sinr_db = 24.391316500000002`

Representative metric origins:

- RTT, RSRP, RSRQ, SINR from `link_model` / `sim_link_model`
- jitter, packet loss, throughput from `flow_monitor` / `sim_flow_performance`

## 4. NR Verification

### Command used

```bash
env \
  NP_SCENARIO_ID=sim-nr-1-mob-none-linkmodelcheck \
  NP_RUN_ID=20260413T190500Z \
  NP_LOG_ROOT="$PWD/logs" \
  UAVS=1 \
  BASE_STATIONS=1 \
  SIM_TIME=5 \
  SECURITY=none \
  MOBILITY=1 \
  LIVE=0 \
  ./sim/ns3/scripts/run_uav_nr.sh
```

### Raw artifacts

Raw run directory:

- `logs/raw/sim-nr-1-mob-none-linkmodelcheck/20260413T190500Z`

Produced files:

- `ns3_nr_flow_monitor.csv`
- `ns3_nr_link_model.csv`
- `ns3_nr_metadata.json`

### Normalized and analysis outputs

- `logs/normalized/sim-nr-1-mob-none-linkmodelcheck/20260413T190500Z/events.csv`
- `logs/normalized/sim-nr-1-mob-none-linkmodelcheck/20260413T190500Z/dataset_manifest.json`
- `logs/analysis/sim-nr-1-mob-none-linkmodelcheck/20260413T190500Z/run_summary.json`
- `logs/analysis/sim-nr-1-mob-none-linkmodelcheck/20260413T190500Z/metric_summary.csv`
- `logs/analysis/sim-nr-1-mob-none-linkmodelcheck/20260413T190500Z/publication_table.md`

### Verified summary behavior

Representative metrics:

- `mean_rtt_ms = 85.13393475000001`
- `p95_rtt_ms = 86.0140312`
- `mean_jitter_ms = 0.0111605`
- `mean_packet_loss_pct = 0.0`
- `mean_throughput_mbps = 0.0011285`
- `mean_rsrp_dbm = -88.23299775000001`
- `mean_rsrq_db = -11.759063`
- `mean_sinr_db = -5.0`

Representative metric origins:

- RTT, RSRP, RSRQ, SINR from `link_model` / `sim_link_model`
- jitter, packet loss, throughput from `flow_monitor` / `sim_flow_performance`

### NR-specific note

In this verified NR run, the FlowMonitor export showed:

- telemetry uplink: `tx_packets=40`, `rx_packets=0`
- control downlink: `tx_packets=8`, `rx_packets=8`

This is an honest simulator result, not an analysis bug. It means the current NR flow-performance summary is dominated by the control flow in this run.

## 5. Metadata Verification

Both verified sim runs produced metadata JSON with explicit family declarations:

- `flow_monitor_measurement_family = sim_flow_performance`
- `link_model_measurement_family = sim_link_model`

This is now true for:

- `logs/raw/sim-lte-1-static-none-linkmodelcheck/20260413T180500Z/ns3_lte_metadata.json`
- `logs/raw/sim-nr-1-mob-none-linkmodelcheck/20260413T190500Z/ns3_nr_metadata.json`

## 6. Publication Interpretation

These simulator runs are strong publication evidence for:

- export schema correctness
- raw artifact layout
- metadata discipline
- analysis reproducibility
- measurement-family separation

They are not publication evidence for:

- field realism
- calibration
- hold-out validation
- sim-to-real agreement

## 7. Relation To Current Calibration Status

The campaign gate remains correctly closed for calibration.

Current campaign output still reports:

- `status = not_ready`
- `scale_up_recommended = false`

Reasons still include:

- missing hold-out completion
- `cal-d050-r1`, `cal-routeA-r1`, and `cal-pairA-r1` below the minimum structural overlap gate
- those same pairs also below the minimum scientifically comparable overlap gate

Those findings are unchanged by the sim-only fixes in this report.

## 8. Conclusion

The simulator side is now ready for publication-grade simulator experiments and reporting.

The project can continue with:

- additional LTE sim experiments
- additional NR sim experiments
- simulator parameter sweeps
- simulator-only methods/results drafting

The project should not claim calibration readiness until a real hardware anchor run exists.
