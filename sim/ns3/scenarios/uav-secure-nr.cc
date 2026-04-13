#include "ns3/antenna-module.h"
#include "ns3/applications-module.h"
#include "ns3/buildings-module.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/nr-module.h"
#include "ns3/point-to-point-module.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cmath>
#include <exception>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <vector>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UavSecureNr");

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

struct LinkModelSample
{
    uint32_t uavId;
    double simTimeSeconds;
    LiveLinkMetrics metrics;
};

struct PhaseTiming
{
    std::string name;
    double durationSeconds;
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

Vector
BuildOverlappedGridPosition(uint32_t index,
                            uint32_t gridColumns,
                            double nominalSpacingMeters,
                            double overlapSpacingMinMeters,
                            double overlapSpacingMaxMeters,
                            double jitterMeters,
                            double heightMeters,
                            Ptr<UniformRandomVariable> uniform)
{
    const uint32_t row = index / gridColumns;
    const uint32_t col = index % gridColumns;

    double x = col * nominalSpacingMeters;
    double y = row * nominalSpacingMeters;

    if (col > 0 && (col % 2 == 1))
    {
        const double overlappedSpacing =
            uniform->GetValue(overlapSpacingMinMeters, overlapSpacingMaxMeters);
        x -= std::max(0.0, nominalSpacingMeters - overlappedSpacing);
    }
    if (row > 0 && (row % 2 == 1))
    {
        const double overlappedSpacing =
            uniform->GetValue(overlapSpacingMinMeters, overlapSpacingMaxMeters);
        y -= std::max(0.0, nominalSpacingMeters - overlappedSpacing);
    }

    x += uniform->GetValue(-jitterMeters, jitterMeters);
    y += uniform->GetValue(-jitterMeters, jitterMeters);
    return Vector(x, y, heightMeters);
}

std::string
EnvString(const char* name, const std::string& fallback = {})
{
    const char* value = std::getenv(name);
    return value != nullptr ? std::string(value) : fallback;
}

std::string
SanitizeIdentifier(std::string value, const std::string& fallback)
{
    if (value.empty())
    {
        return fallback;
    }

    for (char& character : value)
    {
        const bool isValid = (character >= 'a' && character <= 'z')
                             || (character >= 'A' && character <= 'Z')
                             || (character >= '0' && character <= '9')
                             || character == '-' || character == '_' || character == '.';
        if (!isValid)
        {
            character = '-';
        }
    }

    return value;
}

std::string
CurrentUtcRunId()
{
    std::time_t now = std::time(nullptr);
    std::tm utcNow{};
    gmtime_r(&now, &utcNow);
    std::ostringstream stream;
    stream << std::put_time(&utcNow, "%Y%m%dT%H%M%SZ");
    return stream.str();
}

bool
TryParseDouble(const std::string& value, double* output)
{
    if (value.empty())
    {
        return false;
    }

    try
    {
        *output = std::stod(value);
        return true;
    }
    catch (const std::exception&)
    {
        return false;
    }
}

void
RecordPhase(std::vector<PhaseTiming>* timings,
            const std::string& name,
            const std::chrono::steady_clock::time_point& start,
            const std::chrono::steady_clock::time_point& end)
{
    const double seconds =
        std::chrono::duration_cast<std::chrono::duration<double>>(end - start).count();
    timings->push_back({name, seconds});
    std::cerr << "[nr-phase] " << name << '=' << std::fixed << std::setprecision(3) << seconds
              << "s" << std::endl;
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

    const double wave = std::sin(simTime * 0.38 + static_cast<double>(uavIndex) * 0.42);
    const double fastWave = std::cos(simTime * 0.74 + static_cast<double>(uavIndex) * 0.17);
    const double distanceFactor = Clamp(bestDistance, 1.0, 2200.0);

    const double rssiDbm = Clamp(-46.0 - 17.0 * std::log10(distanceFactor / 18.0) + wave * 2.8,
                                 -112.0,
                                 -40.0);
    const double rsrpDbm = Clamp(rssiDbm - 11.0 - std::abs(fastWave) * 1.6, -120.0, -52.0);
    const double sinrDb = Clamp(31.0 - distanceFactor / 26.0 + wave * 4.1, -5.0, 35.0);
    const double rsrqDb = Clamp(-4.0 - distanceFactor / 190.0 + fastWave * 1.0, -16.0, -2.0);
    const double ssRsrpDbm = Clamp(rsrpDbm + 2.8 + wave * 0.6, -118.0, -48.0);
    const double ssSinrDb = Clamp(sinrDb + 3.0 + fastWave * 0.8, -3.0, 38.0);
    const double pingMs =
        Clamp(11.0 + distanceFactor / 20.0 + security.overheadBytes * 0.05 + std::abs(wave) * 3.0,
              5.0,
              140.0);
    const double jitterMs = Clamp(0.9 + distanceFactor / 260.0 + std::abs(fastWave) * 2.2, 0.2, 24.0);
    const double throughputMbps =
        Clamp(110.0 - distanceFactor / 12.0 - security.overheadBytes * 0.025 + ssSinrDb * 1.6,
              2.0,
              260.0);
    const double packetLossPct =
        Clamp((std::max(0.0, 10.0 - ssSinrDb) * 0.25) + distanceFactor / 320.0 + std::abs(wave),
              0.0,
              25.0);

    std::string quality = "POOR";
    if (ssRsrpDbm >= -82.0 && ssSinrDb >= 21.0)
    {
        quality = "EXCELLENT";
    }
    else if (ssRsrpDbm >= -93.0 && ssSinrDb >= 14.0)
    {
        quality = "GOOD";
    }
    else if (ssRsrpDbm >= -105.0 && ssSinrDb >= 7.0)
    {
        quality = "FAIR";
    }

    return {"NR",
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
            ssRsrpDbm,
            ssSinrDb};
}

UavStatusSnapshot
EstimateUavStatus(uint32_t uavIndex, double simTime, const LiveLinkMetrics& metrics)
{
    const double batteryEnvelope =
        88.0 - std::fmod(simTime * 0.60 + static_cast<double>(uavIndex) * 1.9, 50.0);
    const double batteryPenalty =
        Clamp(metrics.packetLossPct * 0.42 + std::max(0.0, metrics.distanceMeters - 90.0) / 200.0,
              0.0,
              30.0);
    const double batteryPercentage = Clamp(batteryEnvelope - batteryPenalty, 18.0, 100.0);
    const double batteryVoltage = 10.9 + batteryPercentage * 0.061;
    const double batteryCurrentMilliAmps =
        Clamp(1400.0 + metrics.distanceMeters * 1.4 + metrics.throughputMbps * 24.0
                  + std::abs(std::sin(simTime * 0.8 + uavIndex)) * 520.0,
              950.0,
              6400.0);

    std::string systemStatus = "ACTIVE";
    if (metrics.packetLossPct > 10.0 || metrics.sinrDb < 0.5 || batteryPercentage < 25.0)
    {
        systemStatus = "CRITICAL";
    }
    else if (metrics.packetLossPct > 3.5 || metrics.sinrDb < 7.0 || batteryPercentage < 40.0)
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
            static_cast<double>((index % 9) + 1) / 9.0;
        const double orbitRadius = std::max(16.0, mobilityRadiusMeters * (0.4 + normalizedIndex * 0.6));
        const double phase = index * 1.32471795724;
        const double angularRate = (2.0 * M_PI) / (26.0 + (index % 6) * 6.0);
        const double radialAmplitude = orbitRadius * 0.28;
        const double radialRate = angularRate * 0.55;
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
    const double radius = std::max(10.0,
                                   state.orbitRadiusMeters
                                       + state.radialAmplitudeMeters
                                             * std::sin(state.radialRateRadPerSecond * simTime
                                                        + state.phaseRad));
    const double radialVelocity =
        state.radialAmplitudeMeters * state.radialRateRadPerSecond
        * std::cos(state.radialRateRadPerSecond * simTime + state.phaseRad);
    const double tangentialVelocity = radius * state.angularRateRadPerSecond;
    const double altitude =
        std::max(12.0, state.altitudeMeters + 8.0 * std::sin(0.24 * simTime + state.phaseRad));
    const double verticalVelocity = 8.0 * 0.24 * std::cos(0.24 * simTime + state.phaseRad);

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
                  const std::string& scenarioId,
                  const std::string& runId,
                  const SecurityProfile& security)
        : m_enabled(enabled)
        , m_scenarioId(scenarioId)
        , m_runId(runId)
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
        json << "{\"type\":\"snapshot\",\"scenarioId\":\"" << m_scenarioId
             << "\",\"runId\":\"" << m_runId
             << "\",\"source\":\"ns3_live_publisher\",\"metricOrigin\":\"snapshot_estimate\""
             << ",\"evidenceLayer\":\"ui_visualization_only\",\"rat\":\"" << rat
             << "\",\"simTime\":" << simTime
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
                 << metrics.rsrqDb << ",\"sinrDb\":" << metrics.sinrDb << ",\"ssRsrpDbm\":"
                 << metrics.ssRsrpDbm << ",\"ssSinrDb\":" << metrics.ssSinrDb << "}";
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
    std::string m_scenarioId;
    std::string m_runId;
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
    publisher->PublishSnapshot("nr",
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

void
CaptureLinkModelSamples(std::vector<LinkModelSample>* samples,
                        const std::string* rat,
                        const NodeContainer* antennas,
                        const NodeContainer* uavs,
                        const SecurityProfile* security,
                        const std::vector<UavMotionState>* motionStates,
                        Time interval,
                        Time stopTime)
{
    if (samples == nullptr || rat == nullptr || antennas == nullptr || uavs == nullptr
        || security == nullptr)
    {
        return;
    }

    const double simTime = Simulator::Now().GetSeconds();
    for (uint32_t i = 0; i < uavs->GetN(); ++i)
    {
        Vector position = uavs->Get(i)->GetObject<MobilityModel>()->GetPosition();
        if (motionStates != nullptr && i < motionStates->size())
        {
            position = ComputeUavKinematics(motionStates->at(i), simTime).position;
        }

        samples->push_back(
            {i + 1, simTime, EstimateLinkMetrics(*rat, i, simTime, position, *antennas, *security)});
    }

    if (Simulator::Now() + interval < stopTime)
    {
        Simulator::Schedule(interval,
                            &CaptureLinkModelSamples,
                            samples,
                            rat,
                            antennas,
                            uavs,
                            security,
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
                const std::string& scenarioId,
                const std::string& runId,
                const std::string& rat,
                const SecurityProfile& security,
                uint32_t uavs,
                uint32_t baseStations,
                double simTimeSeconds,
                Ptr<Ipv4FlowClassifier> classifier,
                const FlowMonitor::FlowStatsContainer& stats,
                uint16_t telemetryPort)
{
    std::filesystem::create_directories(std::filesystem::path(path).parent_path());

    std::ofstream csv(path, std::ios::out | std::ios::trunc);
    csv << "scenario_id,run_id,source,evidence_layer,metric_origin,rat,security_profile,uavs,"
           "base_stations,sim_time_s,security_overhead_bytes,security_setup_delay_s,flow_id,"
           "flow_type,src_ip,dst_ip,src_port,dst_port,tx_packets,rx_packets,lost_packets,pdr,"
           "throughput_mbps,mean_delay_ms,mean_jitter_ms\n";

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

        csv << scenarioId << ',' << runId << ",ns3_export,simulator_export,flow_monitor,"
            << rat << ',' << security.name << ',' << uavs << ',' << baseStations << ','
            << simTimeSeconds << ',' << security.overheadBytes << ','
            << security.setupDelaySeconds << ',' << flowId << ',' << flowType << ','
            << tuple.sourceAddress << ',' << tuple.destinationAddress
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

    std::cout << "NR summary: flows=" << stats.size() << " measured=" << measuredFlows
              << " security=" << security.name << " overheadBytes=" << security.overheadBytes
              << " setupDelaySeconds=" << security.setupDelaySeconds << '\n';
    if (measuredFlows > 0)
    {
        std::cout << "Average throughput Mbps: " << totalThroughput / measuredFlows << '\n'
                  << "Average delay ms: " << totalDelay / measuredFlows << '\n'
                  << "Average jitter ms: " << totalJitter / measuredFlows << '\n';
    }
}

void
WriteLinkModelCsv(const std::string& path,
                  const std::string& scenarioId,
                  const std::string& runId,
                  const std::string& rat,
                  const SecurityProfile& security,
                  uint32_t uavs,
                  uint32_t baseStations,
                  const std::vector<LinkModelSample>& samples)
{
    std::filesystem::create_directories(std::filesystem::path(path).parent_path());

    std::ofstream csv(path, std::ios::out | std::ios::trunc);
    csv << "scenario_id,run_id,source,evidence_layer,metric_origin,measurement_family,rat,"
           "security_profile,uavs,base_stations,sim_time_s,security_overhead_bytes,"
           "security_setup_delay_s,uav_id,network_type,quality,serving_label,ping_ms,"
           "jitter_ms,packet_loss_pct,throughput_mbps,rssi_dbm,rsrp_dbm,rsrq_db,sinr_db\n";

    for (const auto& sample : samples)
    {
        csv << scenarioId << ',' << runId
            << ",ns3_export,simulator_export,link_model,sim_link_model," << rat << ','
            << security.name << ',' << uavs << ',' << baseStations << ',' << std::fixed
            << std::setprecision(6) << sample.simTimeSeconds << ',' << security.overheadBytes
            << ',' << security.setupDelaySeconds << ',' << sample.uavId << ','
            << sample.metrics.networkType << ',' << sample.metrics.quality << ','
            << sample.metrics.servingLabel << ',' << sample.metrics.pingMs << ','
            << sample.metrics.jitterMs << ',' << sample.metrics.packetLossPct << ','
            << sample.metrics.throughputMbps << ',' << sample.metrics.rssiDbm << ','
            << sample.metrics.rsrpDbm << ',' << sample.metrics.rsrqDb << ','
            << sample.metrics.sinrDb << '\n';
    }
}

void
WriteNodePositionsJson(std::ostream& stream, const char* key, const NodeContainer& nodes)
{
    stream << "  \"" << key << "\": [\n";
    for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
        Ptr<MobilityModel> mobilityModel = nodes.Get(i)->GetObject<MobilityModel>();
        Vector position = mobilityModel != nullptr ? mobilityModel->GetPosition() : Vector();
        stream << "    {\"node_index\": " << i << ", \"x_m\": " << position.x
               << ", \"y_m\": " << position.y << ", \"z_m\": " << position.z << "}";
        if (i + 1 != nodes.GetN())
        {
            stream << ',';
        }
        stream << '\n';
    }
    stream << "  ],\n";
}

void
WriteRunMetadata(const std::string& path,
                 const std::string& scenarioId,
                 const std::string& runId,
                 const std::string& rat,
                 const SecurityProfile& security,
                 uint32_t uavs,
                 uint32_t baseStations,
                 double simTimeSeconds,
                 uint32_t rngRun,
                 const std::string& baseStationLayoutMode,
                 double baseStationNominalSpacingMeters,
                 double baseStationJitterMeters,
                 double baseStationOverlapSpacingMinMeters,
                 double baseStationOverlapSpacingMaxMeters,
                 const NodeContainer& baseStationsNodes,
                 double frequencyHz,
                 double bandwidthHz,
                 uint16_t numerology,
                 uint32_t ueAntennaRows,
                 uint32_t ueAntennaColumns,
                 uint32_t gnbAntennaRows,
                 uint32_t gnbAntennaColumns,
                 const std::string& beamformingMethod,
                 uint32_t telemetryPayloadBytes,
                 uint32_t telemetryIntervalMs,
                 uint32_t controlPayloadBytes,
                 uint32_t controlIntervalMs,
                 const std::string& csvPath,
                 const std::string& linkModelCsvPath,
                 const std::string& syncMethod,
                 bool hasSyncOffset,
                 double syncOffsetMs,
                 const std::string& syncNote,
                 const std::vector<PhaseTiming>& phaseTimings)
{
    std::filesystem::create_directories(std::filesystem::path(path).parent_path());

    std::ofstream metadata(path, std::ios::out | std::ios::trunc);
    metadata << "{\n"
             << "  \"schema_name\": \"networkplanner_sim_export\",\n"
             << "  \"schema_version\": 1,\n"
             << "  \"scenario_id\": \"" << scenarioId << "\",\n"
             << "  \"run_id\": \"" << runId << "\",\n"
             << "  \"source\": \"ns3_export\",\n"
             << "  \"rat\": \"" << rat << "\",\n"
             << "  \"security_profile\": \"" << security.name << "\",\n"
             << "  \"uav_count\": " << uavs << ",\n"
             << "  \"base_station_count\": " << baseStations << ",\n"
             << "  \"sim_time_s\": " << simTimeSeconds << ",\n"
             << "  \"rng_run\": " << rngRun << ",\n"
             << "  \"base_station_layout_mode\": \"" << baseStationLayoutMode << "\",\n"
             << "  \"base_station_nominal_spacing_m\": " << baseStationNominalSpacingMeters
             << ",\n"
             << "  \"base_station_jitter_m\": " << baseStationJitterMeters << ",\n"
             << "  \"base_station_overlap_spacing_min_m\": "
             << baseStationOverlapSpacingMinMeters << ",\n"
             << "  \"base_station_overlap_spacing_max_m\": "
             << baseStationOverlapSpacingMaxMeters << ",\n"
             << "  \"frequency_hz\": " << frequencyHz << ",\n"
             << "  \"bandwidth_hz\": " << bandwidthHz << ",\n"
             << "  \"numerology\": " << numerology << ",\n"
             << "  \"ue_antenna_rows\": " << ueAntennaRows << ",\n"
             << "  \"ue_antenna_columns\": " << ueAntennaColumns << ",\n"
             << "  \"gnb_antenna_rows\": " << gnbAntennaRows << ",\n"
             << "  \"gnb_antenna_columns\": " << gnbAntennaColumns << ",\n"
             << "  \"beamforming_method\": \"" << beamformingMethod << "\",\n"
             << "  \"telemetry_payload_bytes\": " << telemetryPayloadBytes << ",\n"
             << "  \"telemetry_interval_ms\": " << telemetryIntervalMs << ",\n"
             << "  \"control_payload_bytes\": " << controlPayloadBytes << ",\n"
             << "  \"control_interval_ms\": " << controlIntervalMs << ",\n"
             << "  \"security_overhead_bytes\": " << security.overheadBytes << ",\n"
             << "  \"security_setup_delay_s\": " << security.setupDelaySeconds << ",\n"
             << "  \"flow_monitor_csv\": \"" << csvPath << "\",\n"
             << "  \"flow_monitor_metric_origin\": \"flow_monitor\",\n"
             << "  \"flow_monitor_evidence_layer\": \"simulator_export\",\n"
             << "  \"flow_monitor_measurement_family\": \"sim_flow_performance\",\n"
             << "  \"link_model_csv\": \"" << linkModelCsvPath << "\",\n"
             << "  \"link_model_metric_origin\": \"link_model\",\n"
             << "  \"link_model_evidence_layer\": \"simulator_export\",\n"
             << "  \"link_model_measurement_family\": \"sim_link_model\",\n"
             << "  \"live_snapshot_metric_origin\": \"snapshot_estimate\",\n"
             << "  \"live_snapshot_evidence_layer\": \"ui_visualization_only\",\n"
             << "  \"sync_method\": \"" << syncMethod << "\",\n";
    WriteNodePositionsJson(metadata, "base_station_positions_m", baseStationsNodes);
    if (hasSyncOffset)
    {
        metadata << "  \"sync_offset_ms\": " << syncOffsetMs << ",\n";
    }
    if (!syncNote.empty())
    {
        metadata << "  \"sync_note\": \"" << syncNote << "\",\n";
    }
    metadata << "  \"phase_timings_s\": {\n";
    for (std::size_t i = 0; i < phaseTimings.size(); ++i)
    {
        metadata << "    \"" << phaseTimings[i].name << "\": " << phaseTimings[i].durationSeconds;
        metadata << (i + 1 < phaseTimings.size() ? ",\n" : "\n");
    }
    metadata << "  },\n";
    metadata << "  \"security_model_note\": "
             << "\"Security overlay models transport overhead and session setup delay, not end-host cryptographic compute cost.\"\n"
             << "}\n";
}

} // namespace

int
main(int argc, char* argv[])
{
    uint32_t uavs = 100;
    uint32_t baseStations = 4;
    double simTimeSeconds = 60.0;
    double altitudeMeters = 120.0;
    double baseStationHeightMeters = 35.0;
    double horizontalDistanceMeters = 500.0;
    double scenarioWidthMeters = 1200.0;
    double scenarioHeightMeters = 1200.0;
    uint32_t telemetryPayloadBytes = 180;
    uint32_t telemetryIntervalMs = 100;
    uint32_t controlPayloadBytes = 96;
    uint32_t controlIntervalMs = 500;
    uint32_t ueAntennaRows = 2;
    uint32_t ueAntennaColumns = 2;
    uint32_t gnbAntennaRows = 4;
    uint32_t gnbAntennaColumns = 4;
    std::string beamformingMethod = "DirectPathBeamforming";
    double frequencyHz = 3.5e9;
    double bandwidthHz = 40e6;
    uint16_t numerology = 1;
    double totalTxPowerDbm = 43.0;
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
    double mobilityRadiusMeters = 70.0;
    double originLatitude = 39.904459;
    double originLongitude = 116.406847;
    std::string scenarioId =
        SanitizeIdentifier(EnvString("NP_SCENARIO_ID", "unspecified-scenario"), "unspecified-scenario");
    std::string runId = SanitizeIdentifier(EnvString("NP_RUN_ID", CurrentUtcRunId()), CurrentUtcRunId());
    std::string logRoot = EnvString("NP_LOG_ROOT", "logs");
    std::string syncMethod = EnvString("NP_SYNC_METHOD", "unspecified");
    std::string syncNote = EnvString("NP_SYNC_NOTE");
    std::string syncOffsetArg = EnvString("NP_SYNC_OFFSET_MS");
    uint32_t rngRun = 1;
    try
    {
        rngRun = static_cast<uint32_t>(std::stoul(EnvString("NP_RNG_RUN", "1")));
    }
    catch (const std::exception&)
    {
        rngRun = 1;
    }
    std::string csvPath;
    std::string linkModelCsvPath;
    std::string metadataPath;

    CommandLine cmd(__FILE__);
    cmd.AddValue("uavs", "Number of UAV endpoints", uavs);
    cmd.AddValue("baseStations", "Number of NR gNodeBs", baseStations);
    cmd.AddValue("simTime", "Simulation time in seconds", simTimeSeconds);
    cmd.AddValue("altitude", "UE altitude in meters", altitudeMeters);
    cmd.AddValue("bsHeight", "gNodeB height in meters", baseStationHeightMeters);
    cmd.AddValue("distance", "Horizontal distance between gNodeBs", horizontalDistanceMeters);
    cmd.AddValue("scenarioWidth", "Scenario width in meters", scenarioWidthMeters);
    cmd.AddValue("scenarioHeight", "Scenario height in meters", scenarioHeightMeters);
    cmd.AddValue("telemetryPayload", "Telemetry payload bytes before security overhead",
                 telemetryPayloadBytes);
    cmd.AddValue("telemetryIntervalMs", "Telemetry emission interval in milliseconds",
                 telemetryIntervalMs);
    cmd.AddValue("controlPayload", "Control payload bytes before security overhead",
                 controlPayloadBytes);
    cmd.AddValue("controlIntervalMs", "Control emission interval in milliseconds",
                 controlIntervalMs);
    cmd.AddValue("ueAntennaRows", "UE antenna array rows", ueAntennaRows);
    cmd.AddValue("ueAntennaColumns", "UE antenna array columns", ueAntennaColumns);
    cmd.AddValue("gnbAntennaRows", "gNodeB antenna array rows", gnbAntennaRows);
    cmd.AddValue("gnbAntennaColumns", "gNodeB antenna array columns", gnbAntennaColumns);
    cmd.AddValue("beamformingMethod",
                 "NR beamforming method: DirectPathBeamforming or Omni",
                 beamformingMethod);
    cmd.AddValue("frequency", "Carrier frequency in Hz", frequencyHz);
    cmd.AddValue("bandwidth", "Channel bandwidth in Hz", bandwidthHz);
    cmd.AddValue("numerology", "NR numerology", numerology);
    cmd.AddValue("txPower", "Total gNodeB transmit power in dBm", totalTxPowerDbm);
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
    cmd.AddValue("scenarioId", "Scenario identifier for publication-grade exports", scenarioId);
    cmd.AddValue("runId", "Run identifier for publication-grade exports", runId);
    cmd.AddValue("logRoot", "Root directory for logs/raw and derived outputs", logRoot);
    cmd.AddValue("syncMethod", "Clock synchronization method description", syncMethod);
    cmd.AddValue("syncOffsetMs", "Optional clock offset estimate in milliseconds", syncOffsetArg);
    cmd.AddValue("syncNote", "Optional synchronization note", syncNote);
    cmd.AddValue("csv", "CSV output path", csvPath);
    cmd.AddValue("linkModelCsv", "Link-model CSV output path", linkModelCsvPath);
    cmd.AddValue("metadata", "Run metadata JSON output path", metadataPath);
    cmd.Parse(argc, argv);

    scenarioId = SanitizeIdentifier(scenarioId, "unspecified-scenario");
    runId = SanitizeIdentifier(runId, CurrentUtcRunId());
    const std::filesystem::path runDirectory =
        std::filesystem::path(logRoot) / "raw" / scenarioId / runId;
    if (csvPath.empty())
    {
        csvPath = (runDirectory / "ns3_nr_flow_monitor.csv").string();
    }
    if (linkModelCsvPath.empty())
    {
        linkModelCsvPath = (runDirectory / "ns3_nr_link_model.csv").string();
    }
    if (metadataPath.empty())
    {
        metadataPath = (runDirectory / "ns3_nr_metadata.json").string();
    }
    double syncOffsetMs = 0.0;
    const bool hasSyncOffset = TryParseDouble(syncOffsetArg, &syncOffsetMs);
    std::vector<PhaseTiming> phaseTimings;
    const auto totalStart = std::chrono::steady_clock::now();
    auto phaseStart = totalStart;

    const SecurityProfile security =
        ResolveSecurityProfile(securityName, securityOverheadBytes, securitySetupDelaySeconds);
    const std::string rat = "nr";

    if (live != 0)
    {
        GlobalValue::Bind("SimulatorImplementationType",
                          StringValue("ns3::RealtimeSimulatorImpl"));
    }

    GridScenarioHelper gridScenario;
    const uint32_t rows = std::max(1u, static_cast<uint32_t>(std::floor(std::sqrt(baseStations))));
    const uint32_t columns = std::max(1u, static_cast<uint32_t>(std::ceil(
                                        static_cast<double>(baseStations) / rows)));
    gridScenario.SetRows(rows);
    gridScenario.SetColumns(columns);
    gridScenario.SetHorizontalBsDistance(horizontalDistanceMeters);
    gridScenario.SetVerticalBsDistance(horizontalDistanceMeters);
    gridScenario.SetBsHeight(baseStationHeightMeters);
    gridScenario.SetUtHeight(altitudeMeters);
    gridScenario.SetSectorization(GridScenarioHelper::SINGLE);
    gridScenario.SetBsNumber(baseStations);
    gridScenario.SetUtNumber(uavs);
    gridScenario.SetScenarioLength(scenarioWidthMeters);
    gridScenario.SetScenarioHeight(scenarioHeightMeters);
    gridScenario.CreateScenario();
    NodeContainer baseStationsContainer = gridScenario.GetBaseStations();
    Ptr<UniformRandomVariable> bsLayoutRv = CreateObject<UniformRandomVariable>();
    const double baseStationJitterMeters = horizontalDistanceMeters * 0.12;
    const double baseStationOverlapSpacingMinMeters = horizontalDistanceMeters * 0.6;
    const double baseStationOverlapSpacingMaxMeters = horizontalDistanceMeters * 0.8;
    for (uint32_t i = 0; i < baseStationsContainer.GetN(); ++i)
    {
        Ptr<MobilityModel> mobilityModel = baseStationsContainer.Get(i)->GetObject<MobilityModel>();
        if (mobilityModel == nullptr)
        {
            continue;
        }

        mobilityModel->SetPosition(BuildOverlappedGridPosition(i,
                                                               columns,
                                                               horizontalDistanceMeters,
                                                               baseStationOverlapSpacingMinMeters,
                                                               baseStationOverlapSpacingMaxMeters,
                                                               baseStationJitterMeters,
                                                               baseStationHeightMeters,
                                                               bsLayoutRv));
    }
    auto phaseEnd = std::chrono::steady_clock::now();
    RecordPhase(&phaseTimings, "create_scenario", phaseStart, phaseEnd);
    phaseStart = phaseEnd;

    Ptr<NrPointToPointEpcHelper> epcHelper = CreateObject<NrPointToPointEpcHelper>();
    Ptr<IdealBeamformingHelper> beamformingHelper = CreateObject<IdealBeamformingHelper>();
    Ptr<NrHelper> nrHelper = CreateObject<NrHelper>();
    nrHelper->SetBeamformingHelper(beamformingHelper);
    nrHelper->SetEpcHelper(epcHelper);

    Ptr<NrChannelHelper> channelHelper = CreateObject<NrChannelHelper>();
    channelHelper->ConfigureFactories("UMa", "Default", "ThreeGpp");
    channelHelper->SetChannelConditionModelAttribute("UpdatePeriod", TimeValue(MilliSeconds(0)));
    channelHelper->SetPathlossAttribute("ShadowingEnabled", BooleanValue(false));

    CcBwpCreator ccBwpCreator;
    CcBwpCreator::SimpleOperationBandConf bandConf(frequencyHz, bandwidthHz, 1);
    OperationBandInfo band = ccBwpCreator.CreateOperationBandContiguousCc(bandConf);
    channelHelper->AssignChannelsToBands({band});
    BandwidthPartInfoPtrVector allBwps = CcBwpCreator::GetAllBwps({band});
    phaseEnd = std::chrono::steady_clock::now();
    RecordPhase(&phaseTimings, "configure_nr_helpers", phaseStart, phaseEnd);
    phaseStart = phaseEnd;

    if (beamformingMethod == "Omni")
    {
        beamformingHelper->SetAttribute(
            "BeamformingMethod",
            TypeIdValue(QuasiOmniDirectPathBeamforming::GetTypeId()));
    }
    else if (beamformingMethod == "DirectPathBeamforming")
    {
        beamformingHelper->SetAttribute("BeamformingMethod",
                                        TypeIdValue(DirectPathBeamforming::GetTypeId()));
    }
    else
    {
        NS_ABORT_MSG("Unsupported beamforming method: " << beamformingMethod);
    }

    nrHelper->SetUeAntennaAttribute("NumRows", UintegerValue(ueAntennaRows));
    nrHelper->SetUeAntennaAttribute("NumColumns", UintegerValue(ueAntennaColumns));
    nrHelper->SetUeAntennaAttribute("AntennaElement",
                                    PointerValue(CreateObject<IsotropicAntennaModel>()));
    nrHelper->SetGnbAntennaAttribute("NumRows", UintegerValue(gnbAntennaRows));
    nrHelper->SetGnbAntennaAttribute("NumColumns", UintegerValue(gnbAntennaColumns));
    nrHelper->SetGnbAntennaAttribute("AntennaElement",
                                     PointerValue(CreateObject<IsotropicAntennaModel>()));
    epcHelper->SetAttribute("S1uLinkDelay", TimeValue(MilliSeconds(10)));

    NetDeviceContainer gnbNetDev =
        nrHelper->InstallGnbDevice(gridScenario.GetBaseStations(), allBwps);
    phaseEnd = std::chrono::steady_clock::now();
    RecordPhase(&phaseTimings, "install_gnb_devices", phaseStart, phaseEnd);
    phaseStart = phaseEnd;
    NetDeviceContainer ueNetDev =
        nrHelper->InstallUeDevice(gridScenario.GetUserTerminals(), allBwps);
    phaseEnd = std::chrono::steady_clock::now();
    RecordPhase(&phaseTimings, "install_ue_devices", phaseStart, phaseEnd);
    phaseStart = phaseEnd;

    for (uint32_t i = 0; i < gnbNetDev.GetN(); ++i)
    {
        NrHelper::GetGnbPhy(gnbNetDev.Get(i), 0)->SetAttribute("Numerology",
                                                               UintegerValue(numerology));
        NrHelper::GetGnbPhy(gnbNetDev.Get(i), 0)->SetTxPower(totalTxPowerDbm);
    }

    auto [remoteHost, remoteHostAddress] = epcHelper->SetupRemoteHost("10Gb/s", 2000, MilliSeconds(5));

    InternetStackHelper internet;
    internet.Install(gridScenario.GetUserTerminals());
    Ipv4InterfaceContainer ueIpIfaces =
        epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueNetDev));
    nrHelper->AttachToClosestGnb(ueNetDev, gnbNetDev);
    phaseEnd = std::chrono::steady_clock::now();
    RecordPhase(&phaseTimings, "attach_and_ip_setup", phaseStart, phaseEnd);
    phaseStart = phaseEnd;

    NodeContainer uavsContainer = gridScenario.GetUserTerminals();

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
        serverApps.Add(controlSink.Install(gridScenario.GetUserTerminals().Get(i)));

        UdpClientHelper telemetryClient(remoteHostAddress, telemetryPort);
        telemetryClient.SetAttribute("Interval", TimeValue(MilliSeconds(telemetryIntervalMs)));
        telemetryClient.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
        telemetryClient.SetAttribute("PacketSize",
                                     UintegerValue(telemetryPayloadBytes + security.overheadBytes));
        clientApps.Add(telemetryClient.Install(gridScenario.GetUserTerminals().Get(i)));

        UdpClientHelper controlClient(ueIpIfaces.GetAddress(i), controlPort);
        controlClient.SetAttribute("Interval", TimeValue(MilliSeconds(controlIntervalMs)));
        controlClient.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
        controlClient.SetAttribute("PacketSize",
                                   UintegerValue(controlPayloadBytes + security.overheadBytes));
        clientApps.Add(controlClient.Install(remoteHost));
    }
    phaseEnd = std::chrono::steady_clock::now();
    RecordPhase(&phaseTimings, "install_applications", phaseStart, phaseEnd);
    phaseStart = phaseEnd;

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
        motionStates = BuildMotionStates(uavsContainer, mobilityRadiusMeters);
        const Time mobilityInterval = MilliSeconds(std::min<uint32_t>(liveIntervalMs, 250));
        Simulator::Schedule(Seconds(0.1), &UpdateUavMotion, &motionStates, mobilityInterval, stopTime);
    }

    FlowMonitorHelper flowmonHelper;
    NodeContainer endpoints;
    endpoints.Add(remoteHost);
    endpoints.Add(uavsContainer);
    Ptr<FlowMonitor> monitor = flowmonHelper.Install(endpoints);
    monitor->SetAttribute("DelayBinWidth", DoubleValue(0.001));
    monitor->SetAttribute("JitterBinWidth", DoubleValue(0.001));
    monitor->SetAttribute("PacketSizeBinWidth", DoubleValue(32));

    LivePublisher livePublisher(
        live != 0, liveHost, livePort, liveMirrorHost, liveMirrorPort, scenarioId, runId, security);
    std::vector<LinkModelSample> linkModelSamples;
    const Time linkModelInterval = MilliSeconds(liveIntervalMs);
    Simulator::Schedule(clientStart,
                        &CaptureLinkModelSamples,
                        &linkModelSamples,
                        &rat,
                        &baseStationsContainer,
                        &uavsContainer,
                        &security,
                        mobility != 0 ? &motionStates : nullptr,
                        linkModelInterval,
                        stopTime);
    if (live != 0)
    {
        const Time liveInterval = MilliSeconds(liveIntervalMs);
        const double antennaRangeMeters = std::max(200.0, horizontalDistanceMeters * 0.5);
        Simulator::Schedule(Seconds(0.1),
                            &ScheduleLiveSnapshot,
                            &livePublisher,
                            &baseStationsContainer,
                            &uavsContainer,
                            antennaRangeMeters,
                            originLatitude,
                            originLongitude,
                            mobility != 0 ? &motionStates : nullptr,
                            liveInterval,
                            stopTime);
    }

    Simulator::Stop(stopTime);
    phaseEnd = std::chrono::steady_clock::now();
    RecordPhase(&phaseTimings, "pre_run_setup", phaseStart, phaseEnd);
    phaseStart = phaseEnd;
    Simulator::Run();
    phaseEnd = std::chrono::steady_clock::now();
    RecordPhase(&phaseTimings, "simulator_run", phaseStart, phaseEnd);
    phaseStart = phaseEnd;

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier =
        DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
    WriteCsvSummary(csvPath,
                    scenarioId,
                    runId,
                    rat,
                    security,
                    uavs,
                    baseStations,
                    simTimeSeconds,
                    classifier,
                    monitor->GetFlowStats(),
                    telemetryPort);
    WriteLinkModelCsv(linkModelCsvPath,
                      scenarioId,
                      runId,
                      rat,
                      security,
                      uavs,
                      baseStations,
                      linkModelSamples);
    phaseEnd = std::chrono::steady_clock::now();
    RecordPhase(&phaseTimings, "write_csv_outputs", phaseStart, phaseEnd);
    RecordPhase(&phaseTimings, "total_wall_clock", totalStart, phaseEnd);
    WriteRunMetadata(metadataPath,
                     scenarioId,
                     runId,
                     rat,
                     security,
                     uavs,
                     baseStations,
                     simTimeSeconds,
                     rngRun,
                     "seeded_overlapped_grid",
                     horizontalDistanceMeters,
                     baseStationJitterMeters,
                     baseStationOverlapSpacingMinMeters,
                     baseStationOverlapSpacingMaxMeters,
                     baseStationsContainer,
                     frequencyHz,
                     bandwidthHz,
                     numerology,
                     ueAntennaRows,
                     ueAntennaColumns,
                     gnbAntennaRows,
                     gnbAntennaColumns,
                     beamformingMethod,
                     telemetryPayloadBytes,
                     telemetryIntervalMs,
                     controlPayloadBytes,
                     controlIntervalMs,
                     csvPath,
                     linkModelCsvPath,
                     syncMethod,
                     hasSyncOffset,
                     syncOffsetMs,
                     syncNote,
                     phaseTimings);

    Simulator::Destroy();
    return 0;
}
