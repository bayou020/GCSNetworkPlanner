#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-module.h"
#include "ns3/lte-module.h"
#include "ns3/mobility-module.h"
#include "ns3/point-to-point-module.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <vector>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UavSecureLte");

namespace
{

struct SecurityProfile
{
    std::string name;
    uint32_t overheadBytes;
    double setupDelaySeconds;
};

struct GeoCoordinate
{
    double latitude;
    double longitude;
};

struct UavMotionState
{
    Ptr<Node> node;
    Vector anchor;
    double altitudeMeters;
    double orbitRadiusMeters;
    double angularRateRadPerSecond;
    double radialAmplitudeMeters;
    double radialRateRadPerSecond;
    double phaseRad;
};

struct LiveLinkMetrics
{
    std::string networkType;
    std::string quality;
    std::string servingLabel;
    double distanceMeters;
    double pingMs;
    double jitterMs;
    double throughputMbps;
    double packetLossPct;
    double rssiDbm;
    double rsrpDbm;
    double rsrqDb;
    double sinrDb;
    double ssRsrpDbm;
    double ssSinrDb;
};

struct UavStatusSnapshot
{
    std::string vehicleType;
    std::string systemStatus;
    double batteryPercentage;
    double batteryVoltage;
    double batteryCurrentMilliAmps;
};

struct UavKinematics
{
    Vector position;
    double headingDegrees;
    double yawRadians;
    double pitchRadians;
    double rollRadians;
};

double
Clamp(double value, double minimum, double maximum)
{
    return std::max(minimum, std::min(value, maximum));
}

GeoCoordinate
ToGeoCoordinate(double originLatitude, double originLongitude, const Vector& position)
{
    constexpr double metersPerDegreeLatitude = 111320.0;
    const double metersPerDegreeLongitude =
        std::max(1.0, metersPerDegreeLatitude * std::cos(originLatitude * M_PI / 180.0));
    return {originLatitude + position.y / metersPerDegreeLatitude,
            originLongitude + position.x / metersPerDegreeLongitude};
}

LiveLinkMetrics
EstimateLinkMetrics(const std::string& rat,
                    uint32_t uavIndex,
                    double simTime,
                    const Vector& uavPosition,
                    const NodeContainer& antennas,
                    const SecurityProfile& security)
{
    uint32_t servingIndex = 0;
    double bestDistance = std::numeric_limits<double>::max();
    for (uint32_t index = 0; index < antennas.GetN(); ++index)
    {
        const Vector antennaPosition = antennas.Get(index)->GetObject<MobilityModel>()->GetPosition();
        const double distance = CalculateDistance(uavPosition, antennaPosition);
        if (distance < bestDistance)
        {
            bestDistance = distance;
            servingIndex = index;
        }
    }

    const double wave = std::sin(simTime * 0.45 + static_cast<double>(uavIndex) * 0.37);
    const double fastWave = std::cos(simTime * 0.82 + static_cast<double>(uavIndex) * 0.21);
    const double distanceFactor = Clamp(bestDistance, 1.0, 2000.0);

    const double rssiDbm = Clamp(-49.0 - 18.0 * std::log10(distanceFactor / 20.0) + wave * 2.5,
                                 -115.0,
                                 -45.0);
    const double rsrpDbm = Clamp(rssiDbm - 8.0 - std::abs(fastWave) * 2.0, -125.0, -55.0);
    const double sinrDb = Clamp(26.0 - distanceFactor / 28.0 + wave * 3.5, -8.0, 30.0);
    const double rsrqDb = Clamp(-5.0 - distanceFactor / 170.0 + fastWave * 1.3, -19.5, -3.0);
    const double pingMs =
        Clamp(18.0 + distanceFactor / 14.0 + security.overheadBytes * 0.08 + std::abs(wave) * 4.0,
              8.0,
              180.0);
    const double jitterMs = Clamp(1.5 + distanceFactor / 220.0 + std::abs(fastWave) * 3.0, 0.3, 30.0);
    const double throughputMbps =
        Clamp(48.0 - distanceFactor / 18.0 - security.overheadBytes * 0.02 + sinrDb * 0.9,
              0.8,
              120.0);
    const double packetLossPct =
        Clamp((std::max(0.0, 8.0 - sinrDb) * 0.35) + distanceFactor / 260.0 + std::abs(fastWave),
              0.0,
              35.0);

    std::string quality = "POOR";
    if (rsrpDbm >= -84.0 && sinrDb >= 20.0)
    {
        quality = "EXCELLENT";
    }
    else if (rsrpDbm >= -95.0 && sinrDb >= 13.0)
    {
        quality = "GOOD";
    }
    else if (rsrpDbm >= -108.0 && sinrDb >= 5.0)
    {
        quality = "FAIR";
    }

    return {"LTE",
            quality,
            "A" + std::to_string(servingIndex + 1),
            bestDistance,
            pingMs,
            jitterMs,
            throughputMbps,
            packetLossPct,
            rssiDbm,
            rsrpDbm,
            rsrqDb,
            sinrDb,
            0.0,
            0.0};
}

UavStatusSnapshot
EstimateUavStatus(uint32_t uavIndex, double simTime, const LiveLinkMetrics& metrics)
{
    const double batteryEnvelope =
        86.0 - std::fmod(simTime * 0.55 + static_cast<double>(uavIndex) * 1.7, 48.0);
    const double batteryPenalty =
        Clamp(metrics.packetLossPct * 0.45 + std::max(0.0, metrics.distanceMeters - 120.0) / 180.0,
              0.0,
              28.0);
    const double batteryPercentage = Clamp(batteryEnvelope - batteryPenalty, 18.0, 100.0);
    const double batteryVoltage = 10.8 + batteryPercentage * 0.060;
    const double batteryCurrentMilliAmps =
        Clamp(1300.0 + metrics.distanceMeters * 1.6 + metrics.throughputMbps * 28.0
                  + std::abs(std::sin(simTime * 0.7 + uavIndex)) * 450.0,
              900.0,
              6200.0);

    std::string systemStatus = "ACTIVE";
    if (metrics.packetLossPct > 12.0 || metrics.sinrDb < 2.0 || batteryPercentage < 25.0)
    {
        systemStatus = "CRITICAL";
    }
    else if (metrics.packetLossPct > 4.0 || metrics.sinrDb < 8.0 || batteryPercentage < 40.0)
    {
        systemStatus = "STANDBY";
    }

    return {"QUADROTOR",
            systemStatus,
            batteryPercentage,
            batteryVoltage,
            batteryCurrentMilliAmps};
}

std::vector<UavMotionState>
BuildMotionStates(const NodeContainer& uavs, double mobilityRadiusMeters)
{
    std::vector<UavMotionState> states;
    states.reserve(uavs.GetN());

    for (uint32_t index = 0; index < uavs.GetN(); ++index)
    {
        const Ptr<MobilityModel> mobility = uavs.Get(index)->GetObject<MobilityModel>();
        const Vector position = mobility->GetPosition();
        const double normalizedIndex =
            static_cast<double>((index % 7) + 1) / 7.0;
        const double orbitRadius = std::max(18.0, mobilityRadiusMeters * (0.45 + normalizedIndex * 0.55));
        const double phase = index * 1.61803398875;
        const double angularRate = (2.0 * M_PI) / (32.0 + (index % 5) * 7.0);
        const double radialAmplitude = orbitRadius * 0.22;
        const double radialRate = angularRate * 0.5;
        states.push_back({uavs.Get(index),
                          position,
                          position.z,
                          orbitRadius,
                          angularRate,
                          radialAmplitude,
                          radialRate,
                          phase});
    }

    return states;
}

UavKinematics
ComputeUavKinematics(const UavMotionState& state, double simTime)
{
    const double angle = state.phaseRad + state.angularRateRadPerSecond * simTime;
    const double radius = std::max(12.0,
                                   state.orbitRadiusMeters
                                       + state.radialAmplitudeMeters
                                             * std::sin(state.radialRateRadPerSecond * simTime
                                                        + state.phaseRad));
    const double radialVelocity =
        state.radialAmplitudeMeters * state.radialRateRadPerSecond
        * std::cos(state.radialRateRadPerSecond * simTime + state.phaseRad);
    const double tangentialVelocity = radius * state.angularRateRadPerSecond;
    const double altitude =
        std::max(10.0, state.altitudeMeters + 6.0 * std::sin(0.18 * simTime + state.phaseRad));
    const double verticalVelocity = 6.0 * 0.18 * std::cos(0.18 * simTime + state.phaseRad);

    const double vx = radialVelocity * std::cos(angle) - tangentialVelocity * std::sin(angle);
    const double vy = radialVelocity * std::sin(angle) + tangentialVelocity * std::cos(angle);
    const double horizontalSpeed = std::max(0.001, std::hypot(vx, vy));
    const double trackRadians = std::atan2(vy, vx);

    double headingDegrees = std::fmod(450.0 - trackRadians * 180.0 / M_PI, 360.0);
    if (headingDegrees < 0.0)
    {
        headingDegrees += 360.0;
    }

    const double yawRadians = headingDegrees * M_PI / 180.0;
    const double pitchRadians = std::atan2(verticalVelocity, horizontalSpeed);
    const double bankMagnitude =
        std::clamp(std::atan2(horizontalSpeed * horizontalSpeed,
                              std::max(radius * 9.81, 0.001)),
                   0.0,
                   M_PI / 6.0);
    const double rollRadians =
        state.angularRateRadPerSecond >= 0.0 ? bankMagnitude : -bankMagnitude;

    return {Vector(state.anchor.x + radius * std::cos(angle),
                   state.anchor.y + radius * std::sin(angle),
                   altitude),
            headingDegrees,
            yawRadians,
            pitchRadians,
            rollRadians};
}

void
UpdateUavMotion(std::vector<UavMotionState>* states, Time interval, Time stopTime)
{
    const double simTime = Simulator::Now().GetSeconds();
    for (auto& state : *states)
    {
        const Ptr<MobilityModel> mobility = state.node->GetObject<MobilityModel>();
        if (mobility == nullptr)
        {
            continue;
        }

        mobility->SetPosition(ComputeUavKinematics(state, simTime).position);
    }

    if (Simulator::Now() + interval < stopTime)
    {
        Simulator::Schedule(interval, &UpdateUavMotion, states, interval, stopTime);
    }
}

class LivePublisher
{
  public:
    LivePublisher(bool enabled,
                  const std::string& host,
                  uint16_t port,
                  const std::string& mirrorHost,
                  uint16_t mirrorPort,
                  const SecurityProfile& security)
        : m_enabled(enabled)
        , m_security(security)
    {
        if (!m_enabled)
        {
            return;
        }

        m_socket = socket(AF_INET, SOCK_DGRAM, 0);
        if (m_socket < 0)
        {
            m_enabled = false;
            std::perror("socket");
            return;
        }

        if (!AppendDestination(host, port))
        {
            close(m_socket);
            m_socket = -1;
            m_enabled = false;
            return;
        }

        if (mirrorPort > 0)
        {
            AppendDestination(mirrorHost.empty() ? host : mirrorHost, mirrorPort);
        }
    }

    ~LivePublisher()
    {
        if (m_socket >= 0)
        {
            close(m_socket);
        }
    }

    void PublishSnapshot(const std::string& rat,
                         double simTime,
                         const NodeContainer& antennas,
                         const NodeContainer& uavs,
                         double antennaRangeMeters,
                         double originLatitude,
                         double originLongitude,
                         const std::vector<UavMotionState>* motionStates) const
    {
        if (!m_enabled || m_socket < 0)
        {
            return;
        }

        std::ostringstream json;
        json << std::fixed << std::setprecision(7);
        json << "{\"type\":\"snapshot\",\"rat\":\"" << rat << "\",\"simTime\":" << simTime
             << ",\"antennas\":[";

        for (uint32_t i = 0; i < antennas.GetN(); ++i)
        {
            const Vector position = antennas.Get(i)->GetObject<MobilityModel>()->GetPosition();
            const GeoCoordinate coordinate =
                ToGeoCoordinate(originLatitude, originLongitude, position);
            if (i > 0)
            {
                json << ',';
            }
            json << "{\"id\":" << i << ",\"label\":\"A" << i + 1 << "\",\"rat\":\"" << rat
                 << "\",\"latitude\":" << coordinate.latitude << ",\"longitude\":"
                 << coordinate.longitude << ",\"heightMeters\":" << position.z
                 << ",\"rangeMeters\":" << antennaRangeMeters << "}";
        }

        json << "],\"uavs\":[";
        for (uint32_t i = 0; i < uavs.GetN(); ++i)
        {
            Vector position = uavs.Get(i)->GetObject<MobilityModel>()->GetPosition();
            double headingDegrees = 0.0;
            double yawRadians = 0.0;
            double pitchRadians = 0.0;
            double rollRadians = 0.0;
            if (motionStates != nullptr && i < motionStates->size())
            {
                const UavKinematics kinematics =
                    ComputeUavKinematics(motionStates->at(i), simTime);
                position = kinematics.position;
                headingDegrees = kinematics.headingDegrees;
                yawRadians = kinematics.yawRadians;
                pitchRadians = kinematics.pitchRadians;
                rollRadians = kinematics.rollRadians;
            }
            const GeoCoordinate coordinate =
                ToGeoCoordinate(originLatitude, originLongitude, position);
            const LiveLinkMetrics metrics =
                EstimateLinkMetrics(rat, i, simTime, position, antennas, m_security);
            const UavStatusSnapshot status = EstimateUavStatus(i, simTime, metrics);
            if (i > 0)
            {
                json << ',';
            }
            json << "{\"id\":" << i << ",\"label\":\"UAV-" << std::setw(3) << std::setfill('0')
                 << i + 1 << "\",\"latitude\":" << coordinate.latitude << ",\"longitude\":"
                 << coordinate.longitude << ",\"altitudeMeters\":" << position.z
                 << ",\"headingDegrees\":" << headingDegrees
                 << ",\"yawRadians\":" << yawRadians << ",\"pitchRadians\":"
                 << pitchRadians << ",\"rollRadians\":" << rollRadians
                 << ",\"vehicleType\":\"" << status.vehicleType << "\",\"systemStatus\":\""
                 << status.systemStatus << "\",\"batteryPercentage\":"
                 << status.batteryPercentage << ",\"batteryVoltage\":"
                 << status.batteryVoltage << ",\"batteryCurrentMilliAmps\":"
                 << status.batteryCurrentMilliAmps
                 << ",\"networkType\":\"" << metrics.networkType << "\",\"quality\":\""
                 << metrics.quality << "\",\"servingLabel\":\"" << metrics.servingLabel
                 << "\",\"security\":\"" << m_security.name << "\",\"distanceMeters\":"
                 << metrics.distanceMeters << ",\"pingMs\":" << metrics.pingMs << ",\"jitterMs\":"
                 << metrics.jitterMs << ",\"throughputMbps\":" << metrics.throughputMbps
                 << ",\"packetLossPct\":" << metrics.packetLossPct << ",\"rssiDbm\":"
                 << metrics.rssiDbm << ",\"rsrpDbm\":" << metrics.rsrpDbm << ",\"rsrqDb\":"
                 << metrics.rsrqDb << ",\"sinrDb\":" << metrics.sinrDb << "}";
        }
        json << "]}";

        const std::string payload = json.str();
        for (const sockaddr_in& address : m_destinations)
        {
            sendto(m_socket,
                   payload.data(),
                   payload.size(),
                   0,
                   reinterpret_cast<const sockaddr*>(&address),
                   sizeof(address));
        }
    }

  private:
    bool AppendDestination(const std::string& host, uint16_t port)
    {
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons(port);
        if (inet_pton(AF_INET, host.c_str(), &address.sin_addr) != 1)
        {
            std::cerr << "Invalid live simulation host: " << host << '\n';
            return false;
        }

        m_destinations.push_back(address);
        return true;
    }

    mutable bool m_enabled = false;
    mutable int m_socket = -1;
    mutable std::vector<sockaddr_in> m_destinations;
    SecurityProfile m_security;
};

void
ScheduleLiveSnapshot(LivePublisher* publisher,
                     const NodeContainer* antennas,
                     const NodeContainer* uavs,
                     double antennaRangeMeters,
                     double originLatitude,
                     double originLongitude,
                     const std::vector<UavMotionState>* motionStates,
                     Time interval,
                     Time stopTime)
{
    publisher->PublishSnapshot("lte",
                               Simulator::Now().GetSeconds(),
                               *antennas,
                               *uavs,
                               antennaRangeMeters,
                               originLatitude,
                               originLongitude,
                               motionStates);

    if (Simulator::Now() + interval < stopTime)
    {
        Simulator::Schedule(interval,
                            &ScheduleLiveSnapshot,
                            publisher,
                            antennas,
                            uavs,
                            antennaRangeMeters,
                            originLatitude,
                            originLongitude,
                            motionStates,
                            interval,
                            stopTime);
    }
}

SecurityProfile
ResolveSecurityProfile(const std::string& requested, uint32_t overrideOverhead, double overrideDelay)
{
    SecurityProfile profile{requested, 0, 0.0};

    if (requested == "wireguard")
    {
        profile.overheadBytes = 60;
        profile.setupDelaySeconds = 0.25;
    }
    else if (requested == "openvpn")
    {
        profile.overheadBytes = 96;
        profile.setupDelaySeconds = 0.75;
    }
    else if (requested == "tls")
    {
        profile.overheadBytes = 25;
        profile.setupDelaySeconds = 0.40;
    }
    else
    {
        profile.name = "none";
    }

    if (overrideOverhead > 0)
    {
        profile.overheadBytes = overrideOverhead;
    }
    if (overrideDelay >= 0.0)
    {
        profile.setupDelaySeconds = overrideDelay;
    }
    return profile;
}

std::string
FlowTypeForTuple(const Ipv4FlowClassifier::FiveTuple& tuple, uint16_t telemetryPort)
{
    if (tuple.destinationPort == telemetryPort)
    {
        return "telemetry-uplink";
    }
    if (tuple.sourcePort == telemetryPort)
    {
        return "telemetry-downlink";
    }
    return "control-downlink";
}

void
WriteCsvSummary(const std::string& path,
                const std::string& rat,
                const SecurityProfile& security,
                uint32_t uavs,
                uint32_t baseStations,
                Ptr<Ipv4FlowClassifier> classifier,
                const FlowMonitor::FlowStatsContainer& stats,
                uint16_t telemetryPort)
{
    std::filesystem::create_directories(std::filesystem::path(path).parent_path());

    std::ofstream csv(path, std::ios::out | std::ios::trunc);
    csv << "rat,security,uavs,base_stations,flow_id,flow_type,src_ip,dst_ip,src_port,dst_port,"
           "tx_packets,rx_packets,lost_packets,pdr,throughput_mbps,mean_delay_ms,mean_jitter_ms\n";

    double totalThroughput = 0.0;
    double totalDelay = 0.0;
    double totalJitter = 0.0;
    uint32_t measuredFlows = 0;

    for (const auto& [flowId, stat] : stats)
    {
        const auto tuple = classifier->FindFlow(flowId);
        const auto flowType = FlowTypeForTuple(tuple, telemetryPort);
        const double duration =
            std::max(1e-9, (stat.timeLastRxPacket - stat.timeFirstTxPacket).GetSeconds());
        const double throughputMbps = stat.rxBytes * 8.0 / duration / 1e6;
        const double meanDelayMs =
            stat.rxPackets > 0 ? stat.delaySum.GetSeconds() * 1000.0 / stat.rxPackets : 0.0;
        const double meanJitterMs =
            stat.rxPackets > 0 ? stat.jitterSum.GetSeconds() * 1000.0 / stat.rxPackets : 0.0;
        const double pdr = stat.txPackets > 0 ? static_cast<double>(stat.rxPackets) / stat.txPackets
                                              : 0.0;

        csv << rat << ',' << security.name << ',' << uavs << ',' << baseStations << ',' << flowId
            << ',' << flowType << ',' << tuple.sourceAddress << ',' << tuple.destinationAddress
            << ',' << tuple.sourcePort << ',' << tuple.destinationPort << ',' << stat.txPackets
            << ',' << stat.rxPackets << ',' << stat.lostPackets << ',' << std::fixed
            << std::setprecision(6) << pdr << ',' << throughputMbps << ',' << meanDelayMs << ','
            << meanJitterMs << '\n';

        if (stat.rxPackets > 0)
        {
            totalThroughput += throughputMbps;
            totalDelay += meanDelayMs;
            totalJitter += meanJitterMs;
            ++measuredFlows;
        }
    }

    std::cout << "LTE summary: flows=" << stats.size() << " measured=" << measuredFlows
              << " security=" << security.name << " overheadBytes=" << security.overheadBytes
              << " setupDelaySeconds=" << security.setupDelaySeconds << '\n';
    if (measuredFlows > 0)
    {
        std::cout << "Average throughput Mbps: " << totalThroughput / measuredFlows << '\n'
                  << "Average delay ms: " << totalDelay / measuredFlows << '\n'
                  << "Average jitter ms: " << totalJitter / measuredFlows << '\n';
    }
}

} // namespace

int
main(int argc, char* argv[])
{
    uint32_t uavs = 100;
    uint32_t baseStations = 4;
    double simTimeSeconds = 60.0;
    double altitudeMeters = 120.0;
    double interSiteDistanceMeters = 750.0;
    double coverageRadiusMeters = 250.0;
    uint16_t downlinkBandwidthRbs = 50;
    uint16_t uplinkBandwidthRbs = 50;
    double enbTxPowerDbm = 30.0;
    uint32_t telemetryPayloadBytes = 180;
    uint32_t telemetryIntervalMs = 100;
    uint32_t controlPayloadBytes = 96;
    uint32_t controlIntervalMs = 500;
    std::string securityName = "wireguard";
    uint32_t securityOverheadBytes = 0;
    double securitySetupDelaySeconds = -1.0;
    uint32_t live = 0;
    std::string liveHost = "127.0.0.1";
    uint16_t livePort = 45454;
    std::string liveMirrorHost;
    uint16_t liveMirrorPort = 0;
    uint32_t liveIntervalMs = 1000;
    uint32_t mobility = 0;
    double mobilityRadiusMeters = 80.0;
    double originLatitude = 39.904459;
    double originLongitude = 116.406847;
    std::string csvPath = "sim/ns3/results/uav-secure-lte.csv";

    CommandLine cmd(__FILE__);
    cmd.AddValue("uavs", "Number of UAV endpoints", uavs);
    cmd.AddValue("baseStations", "Number of LTE eNodeBs", baseStations);
    cmd.AddValue("simTime", "Simulation time in seconds", simTimeSeconds);
    cmd.AddValue("altitude", "UE altitude in meters", altitudeMeters);
    cmd.AddValue("interSiteDistance", "Distance between base stations in meters",
                 interSiteDistanceMeters);
    cmd.AddValue("coverageRadius", "Placement radius around each base station in meters",
                 coverageRadiusMeters);
    cmd.AddValue("dlBandwidth",
                 "LTE downlink bandwidth in resource blocks (6, 15, 25, 50, 75, 100)",
                 downlinkBandwidthRbs);
    cmd.AddValue("ulBandwidth",
                 "LTE uplink bandwidth in resource blocks (6, 15, 25, 50, 75, 100)",
                 uplinkBandwidthRbs);
    cmd.AddValue("txPower", "eNodeB transmit power in dBm", enbTxPowerDbm);
    cmd.AddValue("telemetryPayload", "Telemetry payload bytes before security overhead",
                 telemetryPayloadBytes);
    cmd.AddValue("telemetryIntervalMs", "Telemetry emission interval in milliseconds",
                 telemetryIntervalMs);
    cmd.AddValue("controlPayload", "Control payload bytes before security overhead",
                 controlPayloadBytes);
    cmd.AddValue("controlIntervalMs", "Control emission interval in milliseconds",
                 controlIntervalMs);
    cmd.AddValue("security", "Security overlay: none, tls, wireguard, openvpn", securityName);
    cmd.AddValue("securityOverhead", "Override security overhead bytes", securityOverheadBytes);
    cmd.AddValue("securityDelay", "Override security setup delay in seconds",
                 securitySetupDelaySeconds);
    cmd.AddValue("live", "Run with RealtimeSimulatorImpl and publish live GPS snapshots", live);
    cmd.AddValue("liveHost", "Live simulation destination host", liveHost);
    cmd.AddValue("livePort", "Live simulation destination UDP port", livePort);
    cmd.AddValue("liveMirrorHost", "Secondary live simulation destination host", liveMirrorHost);
    cmd.AddValue("liveMirrorPort", "Secondary live simulation destination UDP port", liveMirrorPort);
    cmd.AddValue("liveIntervalMs", "Live snapshot interval in milliseconds", liveIntervalMs);
    cmd.AddValue("mobility", "Move UAVs during the simulation (0/1)", mobility);
    cmd.AddValue("mobilityRadius", "Radius in meters for UAV orbit-style movement",
                 mobilityRadiusMeters);
    cmd.AddValue("originLat", "GPS origin latitude for projected positions", originLatitude);
    cmd.AddValue("originLon", "GPS origin longitude for projected positions", originLongitude);
    cmd.AddValue("csv", "CSV output path", csvPath);
    cmd.Parse(argc, argv);

    const SecurityProfile security =
        ResolveSecurityProfile(securityName, securityOverheadBytes, securitySetupDelaySeconds);

    if (live != 0)
    {
        GlobalValue::Bind("SimulatorImplementationType",
                          StringValue("ns3::RealtimeSimulatorImpl"));
    }

    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);
    lteHelper->SetEnbDeviceAttribute("DlBandwidth", UintegerValue(downlinkBandwidthRbs));
    lteHelper->SetEnbDeviceAttribute("UlBandwidth", UintegerValue(uplinkBandwidthRbs));

    Ptr<Node> pgw = epcHelper->GetPgwNode();

    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);
    Ptr<Node> remoteHost = remoteHostContainer.Get(0);

    InternetStackHelper internet;
    internet.Install(remoteHostContainer);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", DataRateValue(DataRate("10Gb/s")));
    p2p.SetDeviceAttribute("Mtu", UintegerValue(2000));
    p2p.SetChannelAttribute("Delay", TimeValue(MilliSeconds(10)));

    NetDeviceContainer internetDevices = p2p.Install(pgw, remoteHost);
    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer internetIfaces = ipv4h.Assign(internetDevices);
    Ipv4Address remoteHostAddress = internetIfaces.GetAddress(1);

    Ipv4StaticRoutingHelper routingHelper;
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting =
        routingHelper.GetStaticRouting(remoteHost->GetObject<Ipv4>());
    remoteHostStaticRouting->AddNetworkRouteTo(Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1);

    NodeContainer enbNodes;
    enbNodes.Create(baseStations);
    NodeContainer ueNodes;
    ueNodes.Create(uavs);

    Ptr<ListPositionAllocator> enbPositions = CreateObject<ListPositionAllocator>();
    const uint32_t gridColumns = std::ceil(std::sqrt(static_cast<double>(baseStations)));
    for (uint32_t i = 0; i < baseStations; ++i)
    {
        const uint32_t row = i / gridColumns;
        const uint32_t col = i % gridColumns;
        enbPositions->Add(Vector(col * interSiteDistanceMeters, row * interSiteDistanceMeters, 30.0));
    }

    MobilityHelper enbMobility;
    enbMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    enbMobility.SetPositionAllocator(enbPositions);
    enbMobility.Install(enbNodes);

    Ptr<ListPositionAllocator> uePositions = CreateObject<ListPositionAllocator>();
    for (uint32_t i = 0; i < uavs; ++i)
    {
        const uint32_t servingCell = i % baseStations;
        const Vector anchor = enbNodes.Get(servingCell)->GetObject<MobilityModel>()->GetPosition();
        const double angle = std::fmod(i * 137.50776405003785, 360.0) * M_PI / 180.0;
        const double radialFraction = ((i / baseStations) + 1.0) / ((uavs / baseStations) + 2.0);
        const double radius = std::min(coverageRadiusMeters, coverageRadiusMeters * radialFraction);
        uePositions->Add(Vector(anchor.x + radius * std::cos(angle),
                                anchor.y + radius * std::sin(angle),
                                altitudeMeters));
    }

    MobilityHelper ueMobility;
    ueMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    ueMobility.SetPositionAllocator(uePositions);
    ueMobility.Install(ueNodes);

    NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes);
    for (uint32_t i = 0; i < enbLteDevs.GetN(); ++i)
    {
        Ptr<LteEnbNetDevice> enbDevice = DynamicCast<LteEnbNetDevice>(enbLteDevs.Get(i));
        if (enbDevice != nullptr)
        {
            enbDevice->GetPhy()->SetAttribute("TxPower", DoubleValue(enbTxPowerDbm));
        }
    }

    internet.Install(ueNodes);
    Ipv4InterfaceContainer ueIpIfaces = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueLteDevs));
    for (uint32_t i = 0; i < uavs; ++i)
    {
        Ptr<Ipv4StaticRouting> ueStaticRouting =
            routingHelper.GetStaticRouting(ueNodes.Get(i)->GetObject<Ipv4>());
        ueStaticRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);
        lteHelper->Attach(ueLteDevs.Get(i), enbLteDevs.Get(i % baseStations));
    }

    uint16_t telemetryPort = 9000;
    uint16_t controlBasePort = 10000;

    ApplicationContainer serverApps;
    ApplicationContainer clientApps;

    PacketSinkHelper telemetrySink("ns3::UdpSocketFactory",
                                   InetSocketAddress(Ipv4Address::GetAny(), telemetryPort));
    serverApps.Add(telemetrySink.Install(remoteHost));

    for (uint32_t i = 0; i < uavs; ++i)
    {
        const uint16_t controlPort = controlBasePort + static_cast<uint16_t>(i);
        PacketSinkHelper controlSink("ns3::UdpSocketFactory",
                                     InetSocketAddress(Ipv4Address::GetAny(), controlPort));
        serverApps.Add(controlSink.Install(ueNodes.Get(i)));

        UdpClientHelper telemetryClient(remoteHostAddress, telemetryPort);
        telemetryClient.SetAttribute("Interval", TimeValue(MilliSeconds(telemetryIntervalMs)));
        telemetryClient.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
        telemetryClient.SetAttribute("PacketSize",
                                     UintegerValue(telemetryPayloadBytes + security.overheadBytes));
        clientApps.Add(telemetryClient.Install(ueNodes.Get(i)));

        UdpClientHelper controlClient(ueIpIfaces.GetAddress(i), controlPort);
        controlClient.SetAttribute("Interval", TimeValue(MilliSeconds(controlIntervalMs)));
        controlClient.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
        controlClient.SetAttribute("PacketSize",
                                   UintegerValue(controlPayloadBytes + security.overheadBytes));
        clientApps.Add(controlClient.Install(remoteHost));
    }

    const Time serverStart = Seconds(0.5);
    const Time clientStart = Seconds(1.0 + security.setupDelaySeconds);
    const Time stopTime = Seconds(simTimeSeconds);
    serverApps.Start(serverStart);
    clientApps.Start(clientStart);
    serverApps.Stop(stopTime);
    clientApps.Stop(stopTime);

    std::vector<UavMotionState> motionStates;
    if (mobility != 0)
    {
        motionStates = BuildMotionStates(ueNodes, mobilityRadiusMeters);
        const Time mobilityInterval = MilliSeconds(std::min<uint32_t>(liveIntervalMs, 250));
        Simulator::Schedule(Seconds(0.1), &UpdateUavMotion, &motionStates, mobilityInterval, stopTime);
    }

    FlowMonitorHelper flowmonHelper;
    NodeContainer endpoints;
    endpoints.Add(remoteHost);
    endpoints.Add(ueNodes);
    Ptr<FlowMonitor> monitor = flowmonHelper.Install(endpoints);
    monitor->SetAttribute("DelayBinWidth", DoubleValue(0.001));
    monitor->SetAttribute("JitterBinWidth", DoubleValue(0.001));
    monitor->SetAttribute("PacketSizeBinWidth", DoubleValue(32));

    LivePublisher livePublisher(live != 0, liveHost, livePort, liveMirrorHost, liveMirrorPort, security);
    if (live != 0)
    {
        const Time liveInterval = MilliSeconds(liveIntervalMs);
        Simulator::Schedule(Seconds(0.1),
                            &ScheduleLiveSnapshot,
                            &livePublisher,
                            &enbNodes,
                            &ueNodes,
                            coverageRadiusMeters,
                            originLatitude,
                            originLongitude,
                            mobility != 0 ? &motionStates : nullptr,
                            liveInterval,
                            stopTime);
    }

    Simulator::Stop(stopTime);
    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier =
        DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
    WriteCsvSummary(csvPath,
                    "lte",
                    security,
                    uavs,
                    baseStations,
                    classifier,
                    monitor->GetFlowStats(),
                    telemetryPort);

    Simulator::Destroy();
    return 0;
}
