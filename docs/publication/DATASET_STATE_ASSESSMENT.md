# Dataset State Assessment — MDPI Drones

**Date:** 2026-04-13
**Assessed against:** MDPI Drones submission requirements
**Scope:** All normalized runs in `logs/normalized/` as of this date

---

## Summary Verdict

**Not yet publishable.** The measurement and analysis infrastructure is complete; the experimental dataset is not. There are now 19 normalized runs, including five larger LTE simulator runs at `100` UAVs / `10` base stations. That improves the simulation side materially, but the evidence is still too narrow: there is no completed full-scale NR run, field security coverage is still absent, and the LTE security-profile cases still have no replication.

---

## Current Dataset Inventory

| Scenario ID | Run ID | Domain | RAT | UAVs | Security | Rows | Evidence layers | Sync |
|---|---|---|---|---|---|---|---|---|
| `field-lte-1-mob-none-routeA` | `20260413T053030Z` | field | LTE | 1 | none | 50 | field_ground_truth, ui_visualization_only | unspecified |
| `field-lte-1-static-none-d050` | `20260413T053029Z` | field | LTE | 1 | none | 38 | field_ground_truth, ui_visualization_only | unspecified |
| `field-lte-2-static-none-pairA` | `20260413T053031Z` | field | LTE | 2 | none | 62 | field_ground_truth, ui_visualization_only | unspecified |
| `field-lte-3-static-none-bridgeproxy` | `20260413T133000Z` | field | LTE | 3 | none | 30 | field_ground_truth, ui_visualization_only | unspecified |
| `field-lte-1-mob-none-verifier` | `20260413T104500Z` | field | LTE | 1 | none | 8 | field_ground_truth, simulator_export | ntp_manual_note / sim_clock |
| `sim-lte-1-mob-none-routeA` | `20260413T053030Z` | sim | LTE | 1 | none | 2 | simulator_export | unspecified |
| `sim-lte-1-static-none-d050` | `20260413T053029Z` | sim | LTE | 1 | none | 2 | simulator_export | unspecified |
| `sim-lte-2-static-none-pairA` | `20260413T053031Z` | sim | LTE | 2 | none | 4 | simulator_export | unspecified |
| `sim-lte-3-static-none-firstpair` | `20260413T133000Z` | sim | LTE | 3 | none | 6 | simulator_export | unspecified |
| `sim-lte-1-mob-none-verifier` | `20260413T104700Z` | sim | LTE | 1 | none | 3 | field_ground_truth, simulator_export | sim_clock |
| `sim-lte-1-static-none-linkmodelcheck` | `20260413T081313Z` | sim | LTE | 1 | none | 6 | simulator_export | unspecified |
| `sim-lte-1-static-none-linkmodelcheck` | `20260413T180500Z` | sim | LTE | 1 | none | 6 | simulator_export | unspecified |
| `sim-lte-100-mob-none-default` | `20260413T082459Z` | sim | LTE | 100 | none | 1100 | simulator_export | unspecified |
| `sim-lte-100-static-none-default` | `20260413T091109Z` | sim | LTE | 100 | none | 1100 | simulator_export | unspecified |
| `sim-lte-100-static-tls-default` | `20260413T085930Z` | sim | LTE | 100 | tls | 1100 | simulator_export | unspecified |
| `sim-lte-100-static-wireguard-default` | `20260413T085931Z` | sim | LTE | 100 | wireguard | 1100 | simulator_export | unspecified |
| `sim-lte-100-static-openvpn-default` | `20260413T091110Z` | sim | LTE | 100 | openvpn | 1100 | simulator_export | unspecified |
| `sim-nr-1-mob-none-linkmodelcheck` | `20260413T081333Z` | sim | NR | 1 | none | 6 | simulator_export | unspecified |
| `sim-nr-1-mob-none-linkmodelcheck` | `20260413T190500Z` | sim | NR | 1 | none | 6 | simulator_export | unspecified |

The two simulator tests executed in this pass were:

- `sim-lte-1-static-none-linkmodelcheck` / `20260413T081313Z`
- `sim-nr-1-mob-none-linkmodelcheck` / `20260413T081333Z`

Both normalized and summarized successfully. They strengthen simulator-path verification, but they do not change the publication verdict because they are still short verification artifacts rather than publication-scale experiment runs.

The larger simulator test executed in this pass was:

- `sim-lte-100-mob-none-default` / `20260413T082459Z`

That run completed, normalized, and summarized successfully with:

- `200` flow-performance rows
- `900` link-model rows
- `1100` normalized rows total

This is the first publication-scale simulator artifact in the dataset. It materially improves the LTE simulator side, but it is still only one condition, one security profile, and one repetition.

Two additional large-topology LTE runs completed in parallel in this pass:

- `sim-lte-100-static-tls-default` / `20260413T085930Z`
- `sim-lte-100-static-wireguard-default` / `20260413T085931Z`

Both completed, normalized, and summarized successfully with:

- `200` flow-performance rows each
- `900` link-model rows each
- `1100` normalized rows each

This is the first secured simulation evidence in the dataset, but it is still LTE-only and single-run.

Two more matched `static` LTE `100x10` runs completed in this pass to close the simulator-side security-profile set:

- `sim-lte-100-static-none-default` / `20260413T091109Z`
- `sim-lte-100-static-openvpn-default` / `20260413T091110Z`

Both completed, normalized, and summarized successfully with:

- `200` flow-performance rows each
- `900` link-model rows each
- `1100` normalized rows each

An extracted four-profile LTE sweep now exists under:

- `logs/analysis/extractions/lte-100-static-security-sweep/20260413T091454Z`

**Raw-only (no normalized output):**
`sim-lte-100-mob-openvpn-default` — 6 attempted run directories; only one contains any artifact (`rpi_bridge_events.jsonl`), no flow monitor CSV, not normalizable.

**Interrupted / incomplete high-scale NR attempts (no usable output):**

- `sim-nr-100-mob-none-default` / `20260413T082646Z`
- `sim-nr-100-static-none-default` / `20260413T083504Z`
- `sim-nr-100-static-none-topologycheck` / `20260413T084019Z`
- `sim-nr-100-static-none-fastprofile` / `20260413T085534Z`

All four were CPU-bound and alive for several minutes but did not emit a finished artifact set in reasonable wall-clock time during this pass. The last attempt used a reduced-cost NR profile (`1x1` UE/gNB antennas and omni beamforming), so the remaining blocker is not just the originally chosen beamforming configuration. That is now a simulator scaling finding, not just an untested path.

---

## Gap Analysis

### 1. No security profile comparison (critical)

The dataset now includes LTE simulation evidence for all four modeled security profiles:

- `none`
- `tls`
- `wireguard`
- `openvpn`

That is meaningful progress, but it is not yet a publishable security comparison. The completed security-profile matrix exists only on the LTE simulator side, it is limited to one fleet size and one repetition per condition, and there is still no matched field evidence for those profiles. The older `sim-lte-100-mob-openvpn-default` raw run remains effectively empty, but it is no longer the only `openvpn` artifact.

**Required:** at minimum one full LTE simulation campaign across all four security profiles at a fixed fleet size, plus matched field runs for the two or three most practically relevant profiles.

### 2. No NR / 5G data beyond calibration (critical)

There are still only two NR normalized runs, both for `sim-nr-1-mob-none-linkmodelcheck` (`20260413T081333Z` and `20260413T190500Z`). Each is still only 6 rows, 5 s sim time, 1 UAV, variant `linkmodelcheck`. There is no field NR run and no completed full-scale NR simulation run. If the paper compares LTE and NR, there is currently no NR evidence to support any result claim.

**Required:** full NR simulation campaign mirroring the LTE campaign; NR field data if hardware is available. Before that, the NR simulator needs a practical large-topology configuration that actually completes at `100` UAVs / `10` base stations in reasonable wall-clock time.

### 3. Simulation runs are calibration artifacts, not experiment runs

Most `sim-*` normalized runs still have 2–6 rows and remain pipeline smoke tests or cross-checks. The main exceptions are now:

- `sim-lte-100-mob-none-default` / `20260413T082459Z`
- `sim-lte-100-static-none-default` / `20260413T091109Z`
- `sim-lte-100-static-tls-default` / `20260413T085930Z`
- `sim-lte-100-static-wireguard-default` / `20260413T085931Z`
- `sim-lte-100-static-openvpn-default` / `20260413T091110Z`

Each produced 1100 normalized rows and counts as a real simulator experiment artifact. That still does not fix the dataset by itself because the larger runs are all LTE and all single-run.

**Required:** full-scale simulation runs for every scenario family, not just one LTE baseline.

### 4. No statistical replication

Only two simulator verification scenarios have a second run (`sim-lte-1-static-none-linkmodelcheck` and `sim-nr-1-mob-none-linkmodelcheck`). The new `100x10` LTE runs are still single-run conditions. Drones reviewers routinely require ≥ 3 repetitions per condition to report means with error bars or confidence intervals, and no publishable condition in the current dataset meets that bar.

**Required:** minimum N = 3 runs per (RAT × security_profile × uav_count × motion) cell that appears in the Results section.

### 5. `sync_method: unspecified` on all field runs

Every field run (`routeA`, `d050`, `pairA`, `bridgeproxy`) has `sync_method: unspecified`. Without a documented and applied synchronisation method, cross-source timestamps cannot be trusted for latency or RTT analysis, and the calibration step cannot be conducted rigorously.

**Required:** confirm and document the actual synchronisation approach (NTP, GPS PPS, sim clock, manual offset) before the next field campaign; record it in the run manifest.

### 6. `parquet_status: not_requested` on all runs

No run has been exported to Parquet. This is not a blocker for submission but is needed before dataset archival (e.g. Zenodo), which many Drones papers now require for reproducibility.

---

## What Is Solid

The following are genuinely complete and require no further work before the experimental campaign:

- **Measurement infrastructure:** GCS + RPi bridge logging, ns-3 publication exports, evidence-layer separation, and the `PublicationLogger` schema are implemented and verified (see `PUBLICATION_GATE_REPORT.md`).
- **Analysis pipeline:** `normalize`, `summarize`, and `compare` subcommands of `analysis/publication_pipeline.py` are exercised end-to-end and produce consistent output.
- **Simulator verification path:** LTE and NR `linkmodelcheck` reruns now normalize and summarize cleanly with the measurement-family split preserved across `sim_link_model` and `sim_flow_performance`.
- **Large-topology LTE baseline:** `sim-lte-100-mob-none-default` demonstrates that the LTE simulator path can emit publication-shaped artifacts at `100` UAVs and `10` base stations.
- **Parallel CPU throughput:** independent LTE `100x10` runs now launch cleanly without rebuild contention via `NS3_SKIP_BUILD=1`, which is enough to batch future LTE security sweeps productively on CPU.
- **Scenario naming and metadata discipline:** the `scenario_id` / `run_id` / `NP_*` env-var convention is established and propagating correctly.
- **Verifier workflow:** `field-lte-1-mob-none-verifier` demonstrates the cross-check methodology (real GCS events alongside ns-3 gNB export) that can serve as the calibration section's methodology narrative.
- **Small-fleet field coverage:** 1–3 UAV LTE field data exists with real GCS and RPi bridge sources; this is usable as a baseline and as calibration input once mirrored simulations are run.

---

## Minimum Path to Submission

In priority order:

1. **Fix sync_method before next field campaign** — record `ntp`, `gps_pps`, or `manual_offset` in every run manifest; do not leave field runs with `unspecified`.
2. **Replicate the LTE security matrix** — keep the completed `100x10 static` four-profile set, add at least one second fleet size, and replicate each LTE condition ≥ 3×.
3. **Stabilize large-topology NR execution** — complete one NR `100` UAV / `10` base-station run successfully before declaring the NR campaign feasible.
4. **Run full NR simulation campaign** — same conditions as LTE once the large-topology runtime issue is under control.
5. **Run matched field campaigns** — at minimum `none` and one secured profile for LTE; NR field if hardware supports it.
6. **Reserve hold-out set before calibrating** — label calibration runs explicitly; do not tune on runs that will appear in the validation table.
7. **Run comparison pipeline on hold-out set** — produce `comparison_table.md` with real data; record MAE and MAPE in the manuscript.
8. **Replicate each condition ≥ 3×** — add repetition index to variant or run_id to keep them distinguishable.
9. **Export Parquet and archive** — run `publication_pipeline.py` with `--parquet` before submission; upload to Zenodo or equivalent.
