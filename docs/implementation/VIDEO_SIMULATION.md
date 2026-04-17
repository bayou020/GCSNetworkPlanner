# Video Simulation

This project can simulate a degraded UAV video link end to end:

1. `NetworkPlannerRpi` generates a synthetic live video feed with GStreamer.
2. The live ns-3 LTE or NR experiment mirrors per-UAV network metrics to the RPi bridge.
3. The RPi bridge shapes the outgoing video stream using those metrics:
   - packet loss increases frame drops
   - ping and jitter increase frame delay
   - low throughput adds extra pressure to the stream
4. The GCS receives the selected UAV stream over localhost UDP and displays it in the right-side video panel.

## Important model note

This is a practical impairment model, not a full RTP or RTSP stack embedded directly inside ns-3.

- ns-3 remains the authoritative source for the live LTE or NR network conditions
- `NetworkPlannerRpi` uses those conditions to decide when to delay or drop JPEG frames
- the GCS video monitor then shows the visible effect of those impairments in real time

That gives you a practical way to study perceived video quality under varying cellular conditions without implementing a full media protocol stack inside the simulator first.

## 4K-equivalent channel traffic

The local JPEG preview path is not a valid way to claim that ns-3 carried a real 4K media stream. For channel-capacity experiments, the repository now also supports a separate `4K-equivalent` application-traffic mode inside ns-3 itself.

That mode:

- adds a constant-rate UDP uplink flow from selected UAVs to the remote host
- records the flow in FlowMonitor CSV output as `video-uplink`
- propagates the configuration into run metadata

It does not pretend to be a full RTP, H.264, or H.265 media stack. It is an equivalent traffic load model for channel-stress experiments.

Default maintained profile:

- `VIDEO_BITRATE_MBPS=25`
- `VIDEO_PAYLOAD_BYTES=1400`
- `VIDEO_STREAM_UAVS=1`

Example:

```bash
cd /home/boots/work/phd/GCSNetworkPlanner
source ./env
START_GCS=0 WITH_RPI=0 RAT=lte UAVS=20 BASE_STATIONS=4 SIM_TIME=120 SECURITY=openvpn \
VIDEO_STREAM_UAVS=1 VIDEO_BITRATE_MBPS=25 VIDEO_PAYLOAD_BYTES=1400 \
./sim/ns3/scripts/run_live_4k_equivalent.sh
```

The resulting FlowMonitor CSV will contain a `video-uplink` flow type that can be analyzed separately from telemetry and control.

## Environment variables

Shared GCS/RPi video endpoint:

- `NPVIDEO_HOST`
  - default `127.0.0.1`
- `NPVIDEO_PORT`
  - default `5600`

RPi-side video generator controls:

- `NPRPI_ENABLE_VIDEO_SIMULATION`
  - `1` enables the simulated GStreamer video sender
- `NPRPI_VIDEO_HOST`
  - defaults to `NPVIDEO_HOST`
- `NPRPI_VIDEO_PORT`
  - defaults to `NPVIDEO_PORT`
- `NPRPI_VIDEO_WIDTH`
  - default `320`
- `NPRPI_VIDEO_HEIGHT`
  - default `180`
- `NPRPI_VIDEO_FPS`
  - default `10`
- `NPRPI_VIDEO_JPEG_QUALITY`
  - default `55`
- `NPRPI_VIDEO_PATTERN`
  - GStreamer `videotestsrc` pattern, default `ball`
- `NPRPI_VIDEO_MAX_DELAY_MS`
  - caps simulated network-induced frame delay, default `220`

## What you see in the GCS

When the live stack is running:

- click a UAV marker on the map
- the right-side video monitor switches to that UAV
- the panel shows:
  - the current decoded frame
  - receive FPS
  - resolution
  - dropped-frame count
  - received-frame count
  - frame age
  - link state

Visible degradation symptoms:

- increasing `DROP`: more simulated frame loss
- increasing `AGE`: frames are arriving later
- lower `FPS`: the stream is degrading or starving
- frozen preview: the selected UAV is effectively stalled by loss and/or delay

## Basic live workflow

Terminal 1, start the GCS:

```bash
cd /home/boots/work/phd/GCSNetworkPlanner
source ./env
./scripts/run.sh
```

Terminal 2, start the live LTE + RPi stack:

```bash
cd /home/boots/work/phd/GCSNetworkPlanner
source ./env
UAVS=20 BASE_STATIONS=4 SIM_TIME=60 LIVE_INTERVAL_MS=500 MOBILITY=1 \
./sim/ns3/scripts/run_live_uav_lte_with_rpi.sh
```

Then:

1. wait for the live UAV markers to appear
2. click a UAV marker
3. inspect the network banner and the video panel together

The selected UAV’s network metrics and video stream now move together.

## Forcing more visible degradation

To make video quality drop more obviously, increase congestion or reduce cell capacity. For example:

```bash
cd /home/boots/work/phd/GCSNetworkPlanner
source ./env
UAVS=100 BASE_STATIONS=2 SIM_TIME=90 LIVE_INTERVAL_MS=400 MOBILITY=1 SECURITY=openvpn \
NPRPI_VIDEO_WIDTH=640 NPRPI_VIDEO_HEIGHT=360 NPRPI_VIDEO_FPS=15 \
./sim/ns3/scripts/run_live_uav_lte_with_rpi.sh
```

Why this is harsher:

- more UAVs share the same radio resources
- fewer base stations increase contention
- `openvpn` adds more secure-tunnel overhead than a lighter profile
- larger frames and higher FPS raise the effective media load

If you want a calmer stream:

```bash
NPRPI_VIDEO_WIDTH=320 NPRPI_VIDEO_HEIGHT=180 NPRPI_VIDEO_FPS=8 \
./sim/ns3/scripts/run_live_uav_lte_with_rpi.sh
```

## NR workflow

Use the NR wrapper the same way:

```bash
cd /home/boots/work/phd/GCSNetworkPlanner
source ./env
UAVS=20 BASE_STATIONS=4 SIM_TIME=60 LIVE_INTERVAL_MS=500 MOBILITY=1 \
./sim/ns3/scripts/run_live_uav_nr_with_rpi.sh
```

## Troubleshooting

If no video appears:

- make sure the GCS is running before the live wrapper starts
- confirm `source ./env` was used in both terminals
- confirm `NPVIDEO_PORT` matches on both sides
- click a UAV marker so the GCS knows which stream to display

If the video panel stays in `WAITING`:

- verify the RPi bridge started with video enabled
- look for:
  - `VideoStreamSimulator streaming to ...`
  - `VideoStreamFeed received first frame for UAV ...`

If the stream is too stable for testing:

- increase `UAVS`
- reduce `BASE_STATIONS`
- raise `NPRPI_VIDEO_WIDTH`, `NPRPI_VIDEO_HEIGHT`, or `NPRPI_VIDEO_FPS`
- choose a heavier `SECURITY` profile such as `openvpn`

## Dense live profile

For publication-oriented dense live runs, the maintained wrappers now derive more aggressive but still stable defaults automatically when `UAVS >= 50`:

- `LIVE_INTERVAL_MS=75`
- `NP_GCS_NS3_UI_UPDATE_MS=50`
- `NPRPI_NS3_GPS_INTERVAL_MS=100`
- `NPRPI_NS3_ATTITUDE_INTERVAL_MS=100`

Recommended command:

```bash
cd /home/boots/work/phd/GCSNetworkPlanner
source ./env
WITH_RPI=1 RAT=lte UAVS=100 BASE_STATIONS=4 SIM_TIME=120 SECURITY=openvpn \
./sim/ns3/scripts/run_live_network_planner_100x4.sh
```

On macOS, keep the simulated video at the default `320x180` unless you are explicitly testing video stress. That is the most reliable local profile for long dense runs.
