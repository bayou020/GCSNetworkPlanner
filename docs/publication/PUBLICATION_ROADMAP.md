# Publication Roadmap

This document plans the publishable papers achievable from the GCSNetworkPlanner + NetworkPlannerRpi + ns-3 platform. For each paper it specifies the title, contribution, methodology, required experiments, evidence/metrics, target SCI/SCIE journals, and effort estimate.

All target journals listed here are SCIE-indexed (Web of Science). Verify current indexing and impact factor at submission time, since MDPI titles in particular are periodically re-evaluated.

---

## Platform Capabilities Recap

The platform enables, end-to-end:

- desktop Qt/QML GCS with MAVLink C2 and DJI fallback
- Raspberry Pi onboard bridge (real serial/modem or simulated)
- ns-3 LTE/NR live cellular simulation feeding the GCS over UDP
- per-UAV simulated video stream shaped by ns-3 loss/delay
- structured `PublicationLogger` JSONL with stable `scenario_id` / `run_id` / evidence-layer separation
- analysis pipeline (`normalize → summarize → compare`) producing field-vs-sim error tables
- collision-avoidance closed-loop layer (added on the `mac` branch)
- four security profiles modeled at the transport layer: `none`, `tls`, `wireguard`, `openvpn`

Three primitive contributions emerge: a **testbed**, a **dataset/methodology**, and a set of **studies** the testbed enables. Each can sustain at least one paper.

---

## Paper 1 — Cross-Layer Sim-to-Real Testbed (Lead Paper)

### Title

*A Cross-Layer Sim-to-Real Testbed for Cellular UAV Command, Telemetry, and Video Evaluation*

### Contribution

A reproducible evaluation platform combining a real GCS, a Raspberry Pi onboard bridge, and an ns-3 cellular simulation, with explicit evidence-layer discipline that separates field measurements, simulator exports, UI-only metrics, and derived post-processing.

### Research Questions

1. Can a calibrated ns-3 LTE/NR model reproduce field-measured C2 latency, jitter, and loss within a useful error bound?
2. What is the methodology that lets cellular UAV studies stay honest about which numbers came from where?
3. How well does the same calibration generalize across fleet sizes that cannot be flown physically?

### Methodology

1. Field campaign with N≥3 repetitions per scenario family (1-UAV static, 1-UAV mobile, 2-UAV concurrent), LTE only, profile `none`.
2. Mirrored ns-3 runs for the same scenarios, identical payload sizes, control/telemetry intervals, and motion profiles.
3. Calibration on a designated subset; hold-out validation on the remainder.
4. Comparison pipeline produces MAE / MAPE for RTT, jitter, loss, throughput.
5. Scaled simulation campaigns (10–100 UAVs) reported under explicit `simulator_export` labeling.

### Required Experiments

- 3 field scenario families × ≥3 reps = ≥9 field runs
- 3 mirrored sim families × ≥3 reps = ≥9 sim runs
- 1 hold-out set reserved before tuning
- 1 scaled campaign at 50 / 100 UAVs

### Evidence and Metrics

- C2 RTT (mean, p50, p95, p99) — `field_ground_truth` and `simulator_export`
- telemetry continuity (inter-arrival mean, sequence-gap count)
- packet loss, jitter, throughput
- calibration MAE / MAPE per metric
- hold-out validation table

### Figures and Tables

- system architecture diagram
- evidence-layer schema diagram
- calibration vs hold-out parity plots
- per-metric MAE table
- scaled-fleet trend curves

### Status (2026-04-17)

Infrastructure complete (per `PUBLICATION_GATE_REPORT.md`). Field replication, hold-out reservation, and `sync_method` discipline still missing (per `DATASET_STATE_ASSESSMENT.md`).

### Estimated Effort

6–10 weeks of experiment time + 4 weeks writing.

### Target Journals (in priority order)

| Tier | Journal | Indexing | Approx. IF | Fit |
|---|---|---|---|---|
| Primary | **MDPI Drones** | SCIE / Scopus | ~4.4 | Strong — UAV + comms + reproducibility special issues |
| Alternative | **Sensors (MDPI)** | SCIE | ~3.4 | Good — testbed/measurement papers welcomed |
| Stretch | **IEEE Internet of Things Journal** | SCIE | ~10.6 | Possible if the methodology angle is sharpened |
| Stretch | **Computer Networks (Elsevier)** | SCIE | ~5.6 | Reproducibility-of-measurement angle |

---

## Paper 2 — Security-Overlay Overhead on Cellular UAV Fleets

### Title

*Transport-Layer Security Overhead in Cellular UAV Fleets: A Comparative Study of TLS, WireGuard, and OpenVPN under LTE and NR*

### Contribution

A systematic comparison of three secure transport overlays on UAV C2 latency, throughput, jitter, and packet loss across LTE and NR, at fleet sizes from 1 to 100 UAVs.

### Research Questions

1. Which security overlay imposes the lowest C2 latency penalty under each RAT?
2. How does the cost of each overlay scale with fleet size?
3. Does NR's lower air-interface latency cancel out the cryptographic-overlay overhead at the application layer?

### Methodology

1. Reuse the calibrated platform from Paper 1.
2. Run each (RAT × security_profile × fleet_size × motion) cell with N≥3 repetitions.
3. Headline metric set: C2 RTT distribution, telemetry continuity, throughput, sustained loss.
4. Report results as `simulator_export` only; if any field secured runs exist, they appear as a confirmation panel, not the primary evidence.

### Required Experiments

Minimum publishable matrix:

- RAT ∈ {LTE, NR}
- security_profile ∈ {none, tls, wireguard, openvpn}
- fleet_size ∈ {1, 10, 50, 100}
- motion ∈ {static, mobile}
- N = 3 reps per cell

= 2 × 4 × 4 × 2 × 3 = **192 simulation runs**

The dataset already includes 5 LTE 100-UAV runs (one per profile + a mob baseline), so a meaningful subset already exists. The biggest open task is making large-topology NR runs complete in reasonable wall-clock time.

### Evidence and Metrics

- mean and p95 C2 RTT
- telemetry inter-arrival CV (coefficient of variation)
- mean throughput
- packet loss percent
- per-overlay setup-time + steady-state breakdown
- normalized "cost-per-UAV" curves

### Figures and Tables

- box plots of RTT per (RAT, overlay) at each fleet size
- scaling curves (fleet_size on x, overhead on y)
- per-overlay cost decomposition table

### Status

LTE side has one full security sweep at `100x10`. NR is the blocker — large-topology NR scenario currently does not finish.

### Estimated Effort

3–4 weeks of compute time once NR scaling is fixed + 4 weeks writing.

### Target Journals

| Tier | Journal | Indexing | Approx. IF | Fit |
|---|---|---|---|---|
| Primary | **MDPI Drones** | SCIE | ~4.4 | Strong — security in UAV networks is a recurring SI theme |
| Primary alt | **Vehicular Communications (Elsevier)** | SCIE | ~5.8 | Excellent fit — VANET/UAV networking |
| Stretch | **IEEE Transactions on Vehicular Technology** | SCIE | ~6.8 | Tier-1; would need stronger novelty framing |
| Alternative | **IEEE Access** | SCIE | ~3.4 | Broad-scope, faster acceptance |

---

## Paper 3 — Collision-Avoidance Under Cellular Impairment

### Title

*Closed-Loop Collision Avoidance Performance for Cellular-Connected UAV Swarms Under Realistic Network Impairment*

### Contribution

End-to-end evaluation of a collision-avoidance loop where the avoidance command path is exposed to live ns-3-shaped delay, jitter, and loss, quantifying separation-distance degradation as a function of cellular network conditions.

The collision-avoidance code itself is in the platform (`Ns3SimulationFeed::collisionAvoidanceRequested` signal merged from the `mac` branch).

### Research Questions

1. At what RTT does the closed-loop avoidance behavior cross from "safe" to "marginal"?
2. Is the failure mode dominated by command latency, command loss, or telemetry staleness?
3. How does fleet density change the safety envelope?

### Methodology

1. Instrument the collision-avoidance loop to log: trigger time, command tx time, peer separation at trigger, separation at command effect, time-to-resolution.
2. Run sweeps over (UAV density, RAT, security overlay) using the same scenario naming as Papers 1–2.
3. Define and report: minimum separation observed, time-to-resolution, false-trigger rate, missed-trigger rate.
4. Compare against a "no-network-impairment" oracle baseline (direct in-process avoidance) to isolate the network-induced degradation.

### Required Experiments

- density ∈ {10, 25, 50, 100} UAVs in fixed area
- RAT ∈ {LTE, NR}
- overlay ∈ {none, wireguard}
- mobility profile that produces forced near-misses at controlled rates
- N = 5 reps per cell to capture tail behavior

### Evidence and Metrics

- minimum separation distribution per cell
- time-to-resolution percentiles
- command RTT distribution at the moment of trigger
- correlation between command RTT and minimum separation

### Figures and Tables

- safety envelope (separation vs RTT scatter with regression)
- per-density CCDF of minimum separation
- per-overlay loss-vs-resolution-time curves

### Risk

The current collision-avoidance implementation is functional but not yet validated as a research contribution — its decision logic must be documented and defensible, otherwise reviewers will frame it as ad hoc. Add an algorithmic description section.

### Estimated Effort

5–7 weeks (including avoidance-logic write-up + experiments) + 4 weeks writing.

### Target Journals

| Tier | Journal | Indexing | Approx. IF | Fit |
|---|---|---|---|---|
| Primary | **MDPI Drones** | SCIE | ~4.4 | Strong — safety + UAV swarms |
| Primary alt | **IEEE Transactions on Intelligent Transportation Systems** | SCIE | ~7.9 | Good if framed as ITS / aerial mobility |
| Alternative | **MDPI Aerospace** | SCIE | ~2.6 | Solid alternate |
| Stretch | **IEEE Transactions on Aerospace and Electronic Systems** | SCIE | ~4.4 | Tier-1; needs a strong control-theoretic angle |

---

## Paper 4 — Operator-Visible Video Degradation as a Cellular-Health Proxy

### Title

*Operator-Visible Video Degradation as a Lightweight Indicator of Cellular Link Health in UAV Operations*

### Contribution

Empirical mapping between objective ns-3 metrics (loss, delay, jitter) and rendered video quality on the GCS, leading to a lightweight pilot-side indicator that does not require access to modem statistics.

### Research Questions

1. Does perceptual video degradation track cellular metrics tightly enough to substitute for a modem-side health indicator?
2. Which video-quality measure (frame-drop rate, render-side SSIM, latency) correlates best with which network metric?
3. Can a simple binary "link is degrading" signal be triggered from video alone, with low false-positive rate?

### Methodology

1. Use the existing video pipeline: RPi sends file-driven JPEG frames (now supported via `mac` branch additions); GCS displays per-UAV stream with ns-3 loss/delay applied.
2. Sweep loss ∈ {0, 1, 5, 10, 20}%, one-way delay ∈ {20, 50, 100, 200} ms, jitter ∈ {0, 10, 50} ms.
3. At each cell, measure: rendered FPS, frame drop rate, decode latency, and (optionally) SSIM/PSNR against the reference clip.
4. Regress quality measures against network metrics; derive a threshold rule for the proxy indicator.

### Required Experiments

- ~60 sweep cells × N≥3 reps
- 1 reference clip (loop) with known frame structure
- optional: small operator perceptual study (8–12 participants) for face-validity

### Evidence and Metrics

- per-cell mean and CV of rendered FPS
- SSIM / PSNR against reference (if codec allows)
- ROC of "link-degrading" detector
- false-positive / true-positive rates at chosen threshold

### Figures and Tables

- heatmap of rendered FPS vs (loss, delay)
- correlation matrix between video metrics and network metrics
- ROC curve for the proxy detector

### Risk

The video impairment model is metric-shaped, not a real RTP/RTSP stack — reviewers may push back. Frame the contribution as "operator-visible artefact mapping," not "video-codec performance evaluation."

### Estimated Effort

4–6 weeks experiments + 4 weeks writing.

### Target Journals

| Tier | Journal | Indexing | Approx. IF | Fit |
|---|---|---|---|---|
| Primary | **Sensors (MDPI)** | SCIE | ~3.4 | Strong fit — pilot-side sensing/proxies |
| Primary alt | **MDPI Drones** | SCIE | ~4.4 | Possible — UAV operator interfaces |
| Alternative | **Multimedia Tools and Applications (Springer)** | SCIE | ~3.0 | If video-quality angle dominates |
| Alternative | **IEEE Access** | SCIE | ~3.4 | Broad |

---

## Paper 5 — Open Reproducibility Framework (Methodology / Dataset)

### Title

*An Open Evidence-Layered Logging and Analysis Framework for Reproducible UAV-Network Experiments*

### Contribution

The `PublicationLogger` schema, scenario naming convention, evidence-layer separation, and `normalize → summarize → compare` analysis pipeline as a reusable artifact, with the dataset from Papers 1–3 as a worked example.

### Research Questions

1. What schema is required to make UAV-network experiments comparable across labs?
2. Can field-vs-sim calibration be expressed as a stable comparison contract that other testbeds can adopt?

### Methodology

1. Document the schema as a versioned spec (JSON Schema + Markdown).
2. Provide reference implementations: GCS C++, RPi C++, ns-3 export.
3. Provide the analysis pipeline as a standalone Python package.
4. Demonstrate it by re-publishing the Papers 1–3 datasets through it.
5. Show at least one external user (collaborator) reproducing one figure from the released dataset.

### Required Experiments

None new — this paper consumes outputs of Papers 1–3.

### Evidence and Metrics

- schema completeness (coverage of evidence layers)
- reproducibility latency (time from dataset download to reproduced figure)
- adoption signal (citations, forks, external runs) — long-term

### Figures and Tables

- schema diagram
- worked example dataset structure
- reproducibility-flow diagram

### Status

Schema and pipeline already implemented and documented. Main remaining work is packaging, JSON Schema authoring, and Zenodo deposition.

### Risk

Methodology / dataset papers must demonstrate uptake to be credible. Without one external user, reviewers may downgrade this to "tool paper" territory.

### Estimated Effort

3–4 weeks of packaging + 4 weeks writing.

### Target Journals

| Tier | Journal | Indexing | Approx. IF | Fit |
|---|---|---|---|---|
| Primary | **MDPI Data** | ESCI / Scopus | n/a (ESCI not SCIE) | Natural fit but **not SCIE-indexed** at present |
| Primary | **Scientific Data (Nature)** | SCIE | ~9.8 | Strong fit if the dataset is genuinely reusable |
| Primary alt | **SoftwareX (Elsevier)** | SCIE | ~3.0 | Good fit for the analysis pipeline as a software paper |
| Alternative | **MDPI Drones** | SCIE | ~4.4 | Acceptable as a "platform paper" |

> Note: MDPI Data is **not** currently SCIE-indexed; if SCI/SCIE is mandatory, prefer Scientific Data or SoftwareX.

---

## Suggested Submission Sequence

```
Year 1
  Q1–Q2  → Paper 1 (Testbed) — establishes the platform and the dataset
  Q3     → Paper 2 (Security) — first study built on Paper 1
  Q4     → Paper 5 (Methodology / Dataset) — once two studies cite the framework

Year 2
  Q1–Q2  → Paper 3 (Collision Avoidance)
  Q3     → Paper 4 (Video QoE) — opportunistic, lower priority
```

Rationale:

1. Paper 1 must come first because every other paper cites its calibration as evidence the simulator is trustworthy.
2. Papers 2 and 3 reuse the same testbed, so each is "experiments + writing," not "build + experiments + writing."
3. Paper 5 should appear after at least two papers depend on the framework, otherwise reviewers will ask "who else uses this?"
4. Paper 4 is the most disposable — it can be dropped if reviewer feedback on Paper 1 already covers operator-visible degradation.

---

## Cross-Cutting Submission Guidelines

- **Avoid salami slicing.** Each paper must answer a research question that the others do not. The current split is defensible because: Paper 1 is *infrastructure and calibration*, Paper 2 is *security overhead*, Paper 3 is *closed-loop safety*, Paper 4 is *operator perception*, Paper 5 is *reusable framework*.
- **Reuse but distinguish data.** Papers 2–3 cite Paper 1's calibration but use disjoint experiment cells; do not repeat figures.
- **Always state evidence layer.** Every result table must indicate whether values are `field_ground_truth`, `simulator_export`, or `derived_postprocess`.
- **Archive dataset before submission.** Each paper should release its data on Zenodo (or equivalent) and reference the DOI in the manuscript.
- **Pre-register the calibration / hold-out split** before tuning. Reviewers in this area increasingly check for this.

---

## What Could Sink Any of These Papers

1. **Submitting before the field campaign meets `READINESS_CHECKLIST.md`.** Without N≥3 field reps and documented `sync_method`, Paper 1 cannot pass review.
2. **Treating UI-banner numbers as evidence.** All five papers depend on the evidence-layer discipline being intact.
3. **Single-run "results."** Drones reviewers expect error bars; one-shot numbers will be sent back.
4. **Untested security model.** The current ns-3 security overlay is transport-overhead only; explicitly state this limitation in Paper 2.
5. **Closed-loop safety claims without a control-theoretic baseline.** Paper 3 needs a defensible avoidance algorithm description.

---

## Verification

Before declaring any of these papers "ready to draft":

- [ ] all required scenario cells have ≥3 normalized runs
- [ ] hold-out set reserved and untouched during calibration
- [ ] `sync_method` is set (not `unspecified`) on every field run used
- [ ] `comparison_table.md` from `analysis/publication_pipeline.py compare` exists for the relevant pair
- [ ] dataset is exportable via `--parquet` and ready for Zenodo upload
- [ ] limitations section explicitly bounds the security and video models
