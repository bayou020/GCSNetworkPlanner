# ns-3 UAV Cellular Simulation

This workspace adds reproducible `ns-3.47` experiments for UAV-to-GCS communication using the local Raspberry Pi edge model as the endpoint concept.

## What is implemented

- `uav-secure-lte.cc`
  - 4G LTE EPC simulation
  - 100 UAV endpoint default
  - UAV uplink telemetry to the GCS
  - GCS downlink control traffic to each UAV
  - flow-level CSV metrics for throughput, packet delivery ratio, delay, and jitter

- `uav-secure-nr.cc`
  - 5G NR simulation using the `contrib/nr` module included in `ns-allinone-3.47`
  - same traffic model and CSV reporting as the LTE experiment

## What is not implemented

Stock `ns-3.47` does not provide GSM or UMTS/3G models in the local release you downloaded. The `ns-allinone-3.47` archive adds:

- `contrib/nr` for 5G NR
- `contrib/wimax` for WiMAX

It does **not** add GSM or UMTS. For this reason, the current experiment matrix is:

- 4G LTE: supported here
- 5G NR: supported here
- 2G GSM: not supported by the local release
- 3G UMTS: not supported by the local release

## Secure connection model

The scenarios model a secure tunnel as **network overhead plus setup delay**. They do not simulate cryptographic CPU cost.

Supported profiles:

- `none`
- `tls`
- `wireguard`
- `openvpn`

Each profile adds different per-packet tunnel overhead and startup delay so you can compare how the transport behaves under a secured control plane.

## Quick start

```bash
./sim/ns3/scripts/run_uav_lte.sh
./sim/ns3/scripts/run_uav_nr.sh
```

Both commands:

- extract `~/Downloads/ns-allinone-3.47.tar.bz2` into `.deps/` if needed
- copy the repo scenarios into the ns-3 `scratch/` directory
- configure and build ns-3
- run the scenario

## Useful overrides

```bash
UAVS=100 \
BASE_STATIONS=4 \
SIM_TIME=120 \
SECURITY=wireguard \
./sim/ns3/scripts/run_uav_lte.sh
```

```bash
UAVS=100 \
BASE_STATIONS=4 \
SIM_TIME=120 \
SECURITY=openvpn \
./sim/ns3/scripts/run_uav_nr.sh
```

Results are written to:

- `sim/ns3/results/uav-secure-lte.csv`
- `sim/ns3/results/uav-secure-nr.csv`

For publication runs, the helper scripts now default to:

- `logs/raw/<scenario_id>/<run_id>/ns3_lte_flow_monitor.csv`
- `logs/raw/<scenario_id>/<run_id>/ns3_lte_metadata.json`
- `logs/raw/<scenario_id>/<run_id>/ns3_nr_flow_monitor.csv`
- `logs/raw/<scenario_id>/<run_id>/ns3_nr_metadata.json`

The flat `sim/ns3/results/*.csv` path still works if you override `CSV_PATH`, but it is no longer the preferred structure for journal experiments.

## Live QML visualization

The desktop GCS can now consume live ns-3 snapshots on `127.0.0.1:45454` and render:

- UAV markers using GPS coordinates projected from the ns-3 scenario
- antenna markers
- antenna coverage circles
- a live status card showing the current RAT and simulation time
- clickable UAV markers that populate the right-side network banner with live link metrics
- moving UAVs in live runs so the banner values vary while the simulation is active
- a selected-UAV video monitor fed by the RPi bridge and degraded by the live ns-3 metrics

The live transport is UDP/JSON over localhost. The Qt app listens by default on:

- `NS3_SIM_PORT=45454`

When you use the combined `*_with_rpi.sh` helpers, the ns-3 scenario mirrors the same snapshot
stream to a second localhost port for the RPi bridge:

- `NS3_SIM_RPI_PORT=45455`

Run the GCS first:

```bash
./scripts/run.sh
```

Then launch a live ns-3 scenario:

```bash
./sim/ns3/scripts/run_live_uav_lte.sh
./sim/ns3/scripts/run_live_uav_nr.sh
```

If you also want the `NetworkPlannerRpi` simulation repo to consume the same live snapshots
and emit MAVLink telemetry back to the GCS, use the combined stack helpers:

```bash
./sim/ns3/scripts/run_live_uav_lte_with_rpi.sh
./sim/ns3/scripts/run_live_uav_nr_with_rpi.sh
```

Useful overrides:

```bash
LIVE_PORT=45454 \
ORIGIN_LAT=39.904459 \
ORIGIN_LON=116.406847 \
UAVS=25 \
BASE_STATIONS=4 \
SIM_TIME=20 \
MOBILITY=1 \
MOBILITY_RADIUS=80 \
./sim/ns3/scripts/run_live_uav_lte.sh
```

Radio and load tuning also work through environment variables on the live helpers:

- shared traffic load
  - `NS3_TELEMETRY_PAYLOAD`
  - `NS3_TELEMETRY_INTERVAL_MS`
  - `NS3_CONTROL_PAYLOAD`
  - `NS3_CONTROL_INTERVAL_MS`
- LTE
  - `NS3_LTE_TX_POWER`
  - `NS3_LTE_DL_BANDWIDTH`
  - `NS3_LTE_UL_BANDWIDTH`
  - `NS3_LTE_INTERSITE_DISTANCE`
  - `NS3_LTE_COVERAGE_RADIUS`
- NR
  - `NS3_NR_TX_POWER`
  - `NS3_NR_BANDWIDTH`
  - `NS3_NR_FREQUENCY`
  - `NS3_NR_NUMEROLOGY`
  - `NS3_NR_DISTANCE`
  - `NS3_NR_BS_HEIGHT`

Example:

```bash
source ./env
UAVS=20 BASE_STATIONS=4 SIM_TIME=60 \
NS3_LTE_TX_POWER=38 \
NS3_LTE_DL_BANDWIDTH=75 \
NS3_LTE_UL_BANDWIDTH=75 \
NS3_TELEMETRY_PAYLOAD=256 \
NS3_TELEMETRY_INTERVAL_MS=50 \
./sim/ns3/scripts/run_live_uav_lte.sh
```

The `ORIGIN_LAT` and `ORIGIN_LON` values set the geographic reference point used to project the ns-3 meter-based scenario into GPS coordinates for the QML map.

### Live inspection workflow

1. Start the desktop GCS with `./scripts/run.sh`
2. Launch a live LTE or NR run with one of the `run_live_*` scripts
3. Click a UAV marker on the map
4. Inspect the right-side network banner for the currently selected UAV

When the snapshot is driving the map directly, the live banner shows the selected endpoint label,
RAT, ping, RSSI, RSRP, RSRQ, SINR, jitter, loss, throughput, serving antenna, distance, battery,
vehicle type, and state. For NR it also shows `SS-RSRP` and `SS-SINR`.

If you run the `*_with_rpi.sh` helpers, the same live snapshot stream is also consumed by
`NetworkPlannerRpi`, which converts each UAV snapshot into MAVLink heartbeat, SYS_STATUS, and GPS
packets and forwards them over the local pilot UDP channel. That keeps the GCS control path and
the ns-3 fleet view synchronized without forcing the GCS and the RPi bridge to compete for the
same UDP port.

Those same helpers now also let `NetworkPlannerRpi` emit a simulated GStreamer video feed whose
delay and drop behavior are shaped by the live per-UAV ns-3 metrics. The GCS can then display the
selected UAV’s video stream in real time.

See:

- [docs/implementation/VIDEO_SIMULATION.md](/home/boots/work/phd/GCSNetworkPlanner/docs/implementation/VIDEO_SIMULATION.md)

## Research notes

This framework is designed for iterative PhD experiments, not just one-off runs. The obvious next steps are:

- connect traffic rates to the real `NetworkPlannerRpi` message model
- add mobility traces from recorded UAV missions
- add outage, congestion, and handover experiments
- compare secure transport profiles under different radio access technologies

## Publication evidence boundary

For paper-quality analysis:

- use `ns3_*_flow_monitor.csv` and `ns3_*_metadata.json` as simulator evidence
- treat live snapshot metrics as `ui_visualization_only` unless independently corroborated
- treat the security profile model as transport overhead plus setup delay only
