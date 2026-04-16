#include "ns3_simulation_feed.h"
#include "publication_logger.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkDatagram>
#include <QDataStream>
#include <QVariantMap>
#include <QtMath>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace
{

constexpr quint16 kDefaultNs3SimulationPort = 45454;
constexpr char kSnapshotChunkMagic[] = {'N', 'S', '3', 'C'};
constexpr double kDefaultCollisionWarningEnterMeters = 10.0;
constexpr double kDefaultCollisionWarningExitMeters = 12.0;
constexpr double kDefaultCollisionAlertEnterMeters = 6.0;
constexpr double kDefaultCollisionAlertExitMeters = 8.0;
constexpr double kDefaultCollisionSafetyAxisEnterMeters = 5.0;
constexpr double kDefaultCollisionSafetyAxisExitMeters = 6.0;
constexpr int kDefaultCollisionAvoidanceIntervalMs = 350;
constexpr int kDefaultCollisionAvoidanceCommandMagnitude = 760;
constexpr int kDefaultCollisionAvoidanceCommandFloor = 220;

struct CollisionTrackedUav
{
    int rowIndex = -1;
    int id = -1;
    int systemId = -1;
    QString label;
    double latitude = 0.0;
    double longitude = 0.0;
    double altitudeMeters = 0.0;
    double headingDegrees = 0.0;
    double northVelocityMps = 0.0;
    double eastVelocityMps = 0.0;
    double verticalVelocityMps = 0.0;
    bool valid = false;
};

struct CollisionNearestState
{
    double distanceMeters = std::numeric_limits<double>::infinity();
    double horizontalDistanceMeters = std::numeric_limits<double>::infinity();
    double northMeters = 0.0;
    double eastMeters = 0.0;
    double verticalMeters = 0.0;
    double closingSpeedMps = 0.0;
    double timeToClosestSeconds = -1.0;
    double predictedMinDistanceMeters = std::numeric_limits<double>::infinity();
    QString peerLabel;
    int peerId = -1;
    QString severity;
    bool safetyVolumeBreached = false;
};

struct AvoidanceVector
{
    double north = 0.0;
    double east = 0.0;
    double vertical = 0.0;
};

QVariantList toVariantList(const QJsonArray &array)
{
    QVariantList result;
    result.reserve(array.size());
    for (const QJsonValue &value : array)
    {
        result.append(value.toObject().toVariantMap());
    }
    return result;
}

quint16 defaultPortFromEnvironment()
{
    bool ok = false;
    const int configuredPort = qEnvironmentVariableIntValue("NS3_SIM_PORT", &ok);
    if (!ok || configuredPort <= 0 || configuredPort > 65535)
    {
        return kDefaultNs3SimulationPort;
    }
    return static_cast<quint16>(configuredPort);
}

double envDouble(const char *name, double fallback)
{
    if (!qEnvironmentVariableIsSet(name))
    {
        return fallback;
    }

    bool ok = false;
    const double value = qEnvironmentVariable(name).toDouble(&ok);
    return ok ? value : fallback;
}

int defaultSnapshotUiUpdateMs()
{
    bool ok = false;
    const int configuredMs = qEnvironmentVariableIntValue("NP_GCS_NS3_UI_UPDATE_MS", &ok);
    if (!ok) {
        return 75;
    }
    return std::max(0, configuredMs);
}

bool envFlagEnabled(const char *name, bool fallback)
{
    if (!qEnvironmentVariableIsSet(name))
    {
        return fallback;
    }

    const QString value = qEnvironmentVariable(name).trimmed().toLower();
    return value == QStringLiteral("1")
           || value == QStringLiteral("true")
           || value == QStringLiteral("yes")
           || value == QStringLiteral("on");
}

double metersPerLongitudeDegree(double latitudeDegrees)
{
    return std::max(1000.0, 111320.0 * std::cos(qDegreesToRadians(latitudeDegrees)));
}

int collisionSeverityRank(const QString &severity)
{
    if (severity == QStringLiteral("ALERT"))
    {
        return 2;
    }
    if (severity == QStringLiteral("WARNING"))
    {
        return 1;
    }
    return 0;
}

bool withinSafetyBox(double northMeters,
                     double eastMeters,
                     double verticalMeters,
                     double axisMeters)
{
    return std::abs(northMeters) <= axisMeters
           && std::abs(eastMeters) <= axisMeters
           && std::abs(verticalMeters) <= axisMeters;
}

QString collisionSeverityForPair(const QString &previousSeverity,
                                 double distanceMeters,
                                 double northMeters,
                                 double eastMeters,
                                 double verticalMeters,
                                 double warningEnterMeters,
                                 double warningExitMeters,
                                 double alertEnterMeters,
                                 double alertExitMeters,
                                 double safetyAxisEnterMeters,
                                 double safetyAxisExitMeters)
{
    const bool insideEnterBox =
        withinSafetyBox(northMeters, eastMeters, verticalMeters, safetyAxisEnterMeters);
    const bool insideExitBox =
        withinSafetyBox(northMeters, eastMeters, verticalMeters, safetyAxisExitMeters);

    if (previousSeverity == QStringLiteral("ALERT"))
    {
        if (insideExitBox || distanceMeters <= alertExitMeters)
        {
            return QStringLiteral("ALERT");
        }
        if (distanceMeters < warningExitMeters)
        {
            return QStringLiteral("WARNING");
        }
        return QString();
    }

    if (insideEnterBox || distanceMeters <= alertEnterMeters)
    {
        return QStringLiteral("ALERT");
    }

    const double warningThreshold =
        previousSeverity == QStringLiteral("WARNING") ? warningExitMeters : warningEnterMeters;
    if (distanceMeters < warningThreshold)
    {
        return QStringLiteral("WARNING");
    }

    return QString();
}

int clampManualAxis(double value)
{
    return std::clamp(static_cast<int>(std::lround(value)), -1000, 1000);
}

QString pairKeyForSystems(int leftSystemId, int rightSystemId)
{
    const int first = std::min(leftSystemId, rightSystemId);
    const int second = std::max(leftSystemId, rightSystemId);
    return QStringLiteral("%1:%2").arg(first).arg(second);
}

QString collisionActionNameForCommand(int roll, int pitch, int throttle)
{
    if ((std::abs(roll) > 0 || std::abs(pitch) > 0) && std::abs(throttle) > 0)
    {
        return QStringLiteral("COMBINED");
    }

    if (std::abs(throttle) > std::max(std::abs(roll), std::abs(pitch)))
    {
        return QStringLiteral("ASCEND_OR_DESCEND");
    }

    if (std::abs(roll) > 0 || std::abs(pitch) > 0)
    {
        return QStringLiteral("MOVE_HORIZONTALLY");
    }

    return QStringLiteral("REPORT");
}

double wrapDegrees(double degrees)
{
    while (degrees < 0.0)
    {
        degrees += 360.0;
    }
    while (degrees >= 360.0)
    {
        degrees -= 360.0;
    }
    return degrees;
}

double normalizedAxisValue(int value)
{
    return std::clamp(static_cast<double>(value) / 1000.0, -1.0, 1.0);
}

QVariantMap applyManualControlOverlayToRow(const QVariantMap &row,
                                           int roll,
                                           int pitch,
                                           int yaw,
                                           int throttle)
{
    QVariantMap adjusted = row;

    const double rollNorm = normalizedAxisValue(roll);
    const double pitchNorm = normalizedAxisValue(pitch);
    const double yawNorm = normalizedAxisValue(yaw);
    const double throttleNorm = normalizedAxisValue(-throttle);

    double latitude = adjusted.value(QStringLiteral("latitude")).toDouble();
    double longitude = adjusted.value(QStringLiteral("longitude")).toDouble();
    double altitudeMeters = adjusted.value(QStringLiteral("altitudeMeters")).toDouble();
    double headingDegrees = adjusted.value(QStringLiteral("headingDegrees")).toDouble();

    if (!std::isfinite(headingDegrees))
    {
        headingDegrees =
            qRadiansToDegrees(adjusted.value(QStringLiteral("yawRadians")).toDouble());
    }

    headingDegrees = wrapDegrees(headingDegrees + yawNorm * 22.0);
    altitudeMeters = std::max(0.0, altitudeMeters + throttleNorm * 12.0);

    const double headingRadians = qDegreesToRadians(headingDegrees);
    const double forwardMeters = pitchNorm * 10.0;
    const double lateralMeters = rollNorm * 8.0;
    const double northMeters = forwardMeters * std::cos(headingRadians)
                               - lateralMeters * std::sin(headingRadians);
    const double eastMeters = forwardMeters * std::sin(headingRadians)
                              + lateralMeters * std::cos(headingRadians);
    const double metersPerDegreeLatitude = 111320.0;
    const double metersPerDegreeLongitude =
        std::max(1000.0, metersPerDegreeLatitude * std::cos(qDegreesToRadians(latitude)));

    adjusted.insert(QStringLiteral("latitude"),
                    latitude + northMeters / metersPerDegreeLatitude);
    adjusted.insert(QStringLiteral("longitude"),
                    longitude + eastMeters / metersPerDegreeLongitude);
    adjusted.insert(QStringLiteral("altitudeMeters"), altitudeMeters);
    adjusted.insert(QStringLiteral("headingDegrees"), headingDegrees);
    adjusted.insert(QStringLiteral("yawRadians"), qDegreesToRadians(headingDegrees));
    adjusted.insert(QStringLiteral("rollRadians"), rollNorm * 0.45);
    adjusted.insert(QStringLiteral("pitchRadians"), -pitchNorm * 0.35);
    adjusted.insert(QStringLiteral("manualControlOverlay"), true);

    return adjusted;
}

bool decodeChunkedSnapshotDatagram(const QByteArray &datagramData,
                                   Ns3SimulationFeed::PendingChunkedSnapshot &pendingSnapshot,
                                   QByteArray *completedSnapshot)
{
    if (datagramData.size() < 20 || std::memcmp(datagramData.constData(), kSnapshotChunkMagic, 4) != 0)
    {
        return false;
    }

    QDataStream stream(datagramData);
    stream.setByteOrder(QDataStream::BigEndian);

    char magic[4] = {};
    quint32 messageId = 0;
    quint16 chunkIndex = 0;
    quint16 chunkCount = 0;
    quint32 totalSize = 0;
    quint32 chunkSize = 0;
    if (stream.readRawData(magic, 4) != 4)
    {
        return true;
    }
    stream >> messageId >> chunkIndex >> chunkCount >> totalSize >> chunkSize;

    if (chunkCount == 0 || chunkIndex >= chunkCount || totalSize == 0
        || totalSize > 1024 * 1024 || chunkSize == 0 || chunkSize > 65535)
    {
        pendingSnapshot = {};
        return true;
    }

    QByteArray chunkData;
    chunkData.resize(static_cast<int>(chunkSize));
    if (stream.readRawData(chunkData.data(), static_cast<int>(chunkSize)) != static_cast<int>(chunkSize))
    {
        pendingSnapshot = {};
        return true;
    }

    if (pendingSnapshot.messageId != messageId
        || pendingSnapshot.totalSize != static_cast<int>(totalSize)
        || pendingSnapshot.chunks.size() != chunkCount)
    {
        pendingSnapshot = {};
        pendingSnapshot.messageId = messageId;
        pendingSnapshot.totalSize = static_cast<int>(totalSize);
        pendingSnapshot.chunks.resize(chunkCount);
    }

    if (pendingSnapshot.chunks[chunkIndex].isEmpty())
    {
        pendingSnapshot.receivedChunks += 1;
    }
    pendingSnapshot.chunks[chunkIndex] = chunkData;

    if (pendingSnapshot.receivedChunks < pendingSnapshot.chunks.size())
    {
        return true;
    }

    QByteArray reassembled;
    reassembled.reserve(pendingSnapshot.totalSize);
    for (const QByteArray &chunk : std::as_const(pendingSnapshot.chunks))
    {
        if (chunk.isEmpty())
        {
            pendingSnapshot = {};
            return true;
        }
        reassembled.append(chunk);
    }

    pendingSnapshot = {};
    if (reassembled.size() == 0 || reassembled.size() > 1024 * 1024)
    {
        return true;
    }

    *completedSnapshot = reassembled;
    return true;
}

} // namespace

Ns3SnapshotListModel::Ns3SnapshotListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int Ns3SnapshotListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
    {
        return 0;
    }
    return m_rows.size();
}

QVariant Ns3SnapshotListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size())
    {
        return QVariant();
    }

    if (role == ModelDataRole)
    {
        return m_rows.at(index.row());
    }

    return QVariant();
}

QHash<int, QByteArray> Ns3SnapshotListModel::roleNames() const
{
    return {{ModelDataRole, QByteArrayLiteral("modelData")}};
}

void Ns3SnapshotListModel::clear()
{
    if (m_rows.isEmpty())
    {
        return;
    }

    beginResetModel();
    m_rows.clear();
    endResetModel();
}

QString Ns3SnapshotListModel::rowKey(const QVariantMap &row) const
{
    if (row.contains(QStringLiteral("id")))
    {
        return QStringLiteral("id:%1").arg(row.value(QStringLiteral("id")).toString());
    }

    return QStringLiteral("label:%1").arg(row.value(QStringLiteral("label")).toString());
}

void Ns3SnapshotListModel::updateRows(const QVariantList &items)
{
    QList<QVariantMap> normalizedRows;
    normalizedRows.reserve(items.size());
    QStringList normalizedKeys;
    normalizedKeys.reserve(items.size());

    for (const QVariant &entry : items)
    {
        const QVariantMap row = entry.toMap();
        normalizedRows.append(row);
        normalizedKeys.append(rowKey(row));
    }

    QStringList currentKeys;
    currentKeys.reserve(m_rows.size());
    for (const QVariantMap &row : std::as_const(m_rows))
    {
        currentKeys.append(rowKey(row));
    }

    if (currentKeys != normalizedKeys)
    {
        beginResetModel();
        m_rows = normalizedRows;
        endResetModel();
        return;
    }

    for (int index = 0; index < normalizedRows.size(); ++index)
    {
        if (m_rows.at(index) == normalizedRows.at(index))
        {
            continue;
        }

        m_rows[index] = normalizedRows.at(index);
        const QModelIndex changedIndex = createIndex(index, 0);
        emit dataChanged(changedIndex, changedIndex, {ModelDataRole});
    }
}

Ns3SimulationFeed::Ns3SimulationFeed(QObject *parent)
    : QObject(parent)
{
    connect(&m_socket, &QUdpSocket::readyRead, this, &Ns3SimulationFeed::processPendingDatagrams);
    m_snapshotUiUpdateMs = defaultSnapshotUiUpdateMs();
    m_collisionPolicy.warningEnterMeters =
        std::max(0.1, envDouble("NP_GCS_COLLISION_WARNING_ENTER_M",
                                kDefaultCollisionWarningEnterMeters));
    m_collisionPolicy.warningExitMeters =
        std::max(m_collisionPolicy.warningEnterMeters,
                 envDouble("NP_GCS_COLLISION_WARNING_EXIT_M",
                           kDefaultCollisionWarningExitMeters));
    m_collisionPolicy.alertEnterMeters =
        std::max(0.1, envDouble("NP_GCS_COLLISION_ALERT_ENTER_M",
                                kDefaultCollisionAlertEnterMeters));
    m_collisionPolicy.alertExitMeters =
        std::max(m_collisionPolicy.alertEnterMeters,
                 envDouble("NP_GCS_COLLISION_ALERT_EXIT_M",
                           kDefaultCollisionAlertExitMeters));
    m_collisionPolicy.safetyAxisEnterMeters =
        std::max(0.1, envDouble("NP_GCS_COLLISION_SAFETY_AXIS_ENTER_M",
                                kDefaultCollisionSafetyAxisEnterMeters));
    m_collisionPolicy.safetyAxisExitMeters =
        std::max(m_collisionPolicy.safetyAxisEnterMeters,
                 envDouble("NP_GCS_COLLISION_SAFETY_AXIS_EXIT_M",
                           kDefaultCollisionSafetyAxisExitMeters));
    m_collisionPolicy.avoidanceIntervalMs =
        std::max(0, qEnvironmentVariableIntValue("NP_GCS_COLLISION_COMMAND_INTERVAL_MS"));
    if (m_collisionPolicy.avoidanceIntervalMs <= 0)
    {
        m_collisionPolicy.avoidanceIntervalMs = kDefaultCollisionAvoidanceIntervalMs;
    }
    m_collisionPolicy.commandMagnitude =
        std::clamp(qEnvironmentVariableIntValue("NP_GCS_COLLISION_COMMAND_MAGNITUDE"),
                   0,
                   1000);
    if (m_collisionPolicy.commandMagnitude <= 0)
    {
        m_collisionPolicy.commandMagnitude = kDefaultCollisionAvoidanceCommandMagnitude;
    }
    m_collisionPolicy.commandFloor =
        std::clamp(qEnvironmentVariableIntValue("NP_GCS_COLLISION_COMMAND_FLOOR"),
                   0,
                   m_collisionPolicy.commandMagnitude);
    if (m_collisionPolicy.commandFloor <= 0)
    {
        m_collisionPolicy.commandFloor = kDefaultCollisionAvoidanceCommandFloor;
    }
    m_collisionPolicy.sendMavlinkReport =
        envFlagEnabled("NP_GCS_COLLISION_SEND_MAVLINK_REPORT", true);
    m_snapshotApplyTimer.setSingleShot(true);
    connect(&m_snapshotApplyTimer, &QTimer::timeout, this, &Ns3SimulationFeed::applyPendingSnapshot);
    logCollisionPolicyOnce();
}

bool Ns3SimulationFeed::listening() const
{
    return m_listening;
}

bool Ns3SimulationFeed::hasData() const
{
    return m_hasData;
}

QString Ns3SimulationFeed::rat() const
{
    return m_rat;
}

double Ns3SimulationFeed::simTime() const
{
    return m_simTime;
}

quint16 Ns3SimulationFeed::port() const
{
    return m_port;
}

QVariantList Ns3SimulationFeed::antennas() const
{
    return m_antennas;
}

QVariantList Ns3SimulationFeed::uavs() const
{
    return m_uavs;
}

QAbstractItemModel *Ns3SimulationFeed::antennaModel() const
{
    return const_cast<Ns3SnapshotListModel *>(&m_antennaModel);
}

QAbstractItemModel *Ns3SimulationFeed::uavModel() const
{
    return const_cast<Ns3SnapshotListModel *>(&m_uavModel);
}

int Ns3SimulationFeed::antennaCount() const
{
    return m_antennas.size();
}

int Ns3SimulationFeed::uavCount() const
{
    return m_uavs.size();
}

double Ns3SimulationFeed::minAltitudeMeters() const
{
    return m_minAltitudeMeters;
}

double Ns3SimulationFeed::maxAltitudeMeters() const
{
    return m_maxAltitudeMeters;
}

int Ns3SimulationFeed::selectedUavId() const
{
    return m_selectedUavId;
}

bool Ns3SimulationFeed::hasSelectedUav() const
{
    return m_selectedUavId >= 0 && !m_selectedUav.isEmpty();
}

QVariantMap Ns3SimulationFeed::selectedUav() const
{
    return m_selectedUav;
}

QString Ns3SimulationFeed::errorString() const
{
    return m_errorString;
}

int Ns3SimulationFeed::collisionWarningCount() const
{
    return m_collisionWarningCount;
}

int Ns3SimulationFeed::collisionAlertCount() const
{
    return m_collisionAlertCount;
}

QString Ns3SimulationFeed::collisionBannerSeverity() const
{
    return m_collisionBannerSeverity;
}

QString Ns3SimulationFeed::collisionBannerText() const
{
    return m_collisionBannerText;
}

QVariantList Ns3SimulationFeed::collisionPairs() const
{
    return m_collisionPairs;
}

bool Ns3SimulationFeed::startListening(quint16 port)
{
    const quint16 targetPort = port == 0 ? defaultPortFromEnvironment() : port;

    if (m_socket.state() == QAbstractSocket::BoundState && m_port == targetPort)
    {
        setListening(true);
        setErrorString(QString());
        return true;
    }

    if (m_socket.isOpen())
    {
        m_socket.close();
    }

    if (!m_socket.bind(QHostAddress::LocalHost, targetPort,
                       QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint))
    {
        setListening(false);
        setPort(0);
        setErrorString(tr("Failed to bind ns-3 simulation feed on 127.0.0.1:%1: %2")
                           .arg(targetPort)
                           .arg(m_socket.errorString()));
        return false;
    }

    setPort(targetPort);
    setListening(true);
    setErrorString(QString());
    return true;
}

void Ns3SimulationFeed::stopListening()
{
    if (m_socket.isOpen())
    {
        m_socket.close();
    }
    setListening(false);
    setPort(0);
}

void Ns3SimulationFeed::clear()
{
    const bool hadAntennas = !m_antennas.isEmpty();
    const bool hadUavs = !m_uavs.isEmpty();
    m_antennas.clear();
    m_baseUavs.clear();
    m_uavs.clear();
    m_antennaModel.clear();
    m_uavModel.clear();
    if (hadAntennas)
    {
        emit antennasChanged();
    }
    if (hadUavs)
    {
        emit uavsChanged();
    }
    setHasData(false);
    setRat(QString());
    setSimTime(0.0);
    if (!qFuzzyIsNull(m_minAltitudeMeters) || !qFuzzyIsNull(m_maxAltitudeMeters))
    {
        m_minAltitudeMeters = 0.0;
        m_maxAltitudeMeters = 0.0;
        emit altitudeRangeChanged();
    }
    clearSelectedUav();
    setErrorString(QString());
    m_manualControlSystemId = -1;
    m_manualRoll = 0;
    m_manualPitch = 0;
    m_manualYaw = 0;
    m_manualThrottle = 0;
    m_manualControlActive = false;
    resetCollisionTrackingState(true);
}

void Ns3SimulationFeed::selectUavById(int id)
{
    if (id < 0)
    {
        clearSelectedUav();
        return;
    }

    m_selectedUavId = id;
    refreshSelectedUav();
}

void Ns3SimulationFeed::clearSelectedUav()
{
    setSelectedUavState(-1, QVariantMap());
}

void Ns3SimulationFeed::setManualControlOverlay(int systemId,
                                                int roll,
                                                int pitch,
                                                int yaw,
                                                int throttle,
                                                bool active)
{
    const int boundedSystemId = qBound(1, systemId, 250);

    const bool changed = m_manualControlSystemId != boundedSystemId
                         || m_manualRoll != roll
                         || m_manualPitch != pitch
                         || m_manualYaw != yaw
                         || m_manualThrottle != throttle
                         || m_manualControlActive != active;

    if (!changed)
    {
        return;
    }

    m_manualControlSystemId = boundedSystemId;
    m_manualRoll = roll;
    m_manualPitch = pitch;
    m_manualYaw = yaw;
    m_manualThrottle = throttle;
    m_manualControlActive = active;

    if (!m_baseUavs.isEmpty())
    {
        refreshUavDisplayState();
    }
}

void Ns3SimulationFeed::processPendingDatagrams()
{
    QByteArray latestSnapshotPayload;
    while (m_socket.hasPendingDatagrams())
    {
        const QNetworkDatagram datagram = m_socket.receiveDatagram();
        QByteArray completedChunkedSnapshot;
        if (decodeChunkedSnapshotDatagram(datagram.data(),
                                          m_pendingChunkedSnapshot,
                                          &completedChunkedSnapshot))
        {
            if (!completedChunkedSnapshot.isEmpty())
            {
                latestSnapshotPayload = completedChunkedSnapshot;
            }
            continue;
        }

        latestSnapshotPayload = datagram.data();
    }

    if (latestSnapshotPayload.isEmpty()) {
        return;
    }
    m_pendingSnapshotPayload = latestSnapshotPayload;
    m_hasPendingSnapshot = true;

    if (m_snapshotUiUpdateMs <= 0 || !m_snapshotApplyElapsed.isValid()) {
        applyPendingSnapshot();
        return;
    }

    const qint64 elapsedMs = m_snapshotApplyElapsed.elapsed();
    if (elapsedMs >= m_snapshotUiUpdateMs) {
        applyPendingSnapshot();
        return;
    }

    if (!m_snapshotApplyTimer.isActive()) {
        m_snapshotApplyTimer.start(m_snapshotUiUpdateMs - static_cast<int>(elapsedMs));
    }
}

void Ns3SimulationFeed::applyPendingSnapshot()
{
    if (!m_hasPendingSnapshot) {
        return;
    }

    const QByteArray payload = m_pendingSnapshotPayload;
    m_hasPendingSnapshot = false;
    m_pendingSnapshotPayload.clear();
    if (!m_snapshotApplyElapsed.isValid()) {
        m_snapshotApplyElapsed.start();
    } else {
        m_snapshotApplyElapsed.restart();
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
    {
        setErrorString(tr("Invalid ns-3 simulation datagram: %1").arg(parseError.errorString()));
        PublicationLogger::instance().logError(
            QStringLiteral("invalid_ns3_simulation_datagram"),
            {{QStringLiteral("status"), QStringLiteral("parse_error")},
             {QStringLiteral("note"), parseError.errorString()},
             {QStringLiteral("datagram_bytes"), payload.size()}});
        return;
    }

    const QJsonObject snapshot = document.object();
    const QString type = snapshot.value(QStringLiteral("type")).toString();
    if (type != QStringLiteral("snapshot"))
    {
        setErrorString(tr("Unsupported ns-3 simulation message type: %1").arg(type));
        return;
    }

    applySnapshot(snapshot);
}

void Ns3SimulationFeed::applySnapshot(const QJsonObject &snapshot)
{
    const QString rat = snapshot.value(QStringLiteral("rat")).toString();
    const double simTimeSeconds = snapshot.value(QStringLiteral("simTime")).toDouble();
    const QJsonArray antennaArray = snapshot.value(QStringLiteral("antennas")).toArray();
    const QJsonArray uavArray = snapshot.value(QStringLiteral("uavs")).toArray();
    const QVariantList antennas = toVariantList(antennaArray);
    m_baseUavs = toVariantList(uavArray);

    PublicationLogger::instance().logEvent(
        QStringLiteral("sim_snapshot"),
        {{QStringLiteral("status"), QStringLiteral("ingested")},
         {QStringLiteral("message_name"), QStringLiteral("NS3_SNAPSHOT")},
         {QStringLiteral("rat"), rat},
         {QStringLiteral("sim_time_s"), simTimeSeconds},
         {QStringLiteral("uav_count"), uavArray.size()},
         {QStringLiteral("antenna_count"), antennaArray.size()},
         {QStringLiteral("metric_origin"), QStringLiteral("snapshot_estimate")},
         {QStringLiteral("evidence_layer"), QStringLiteral("ui_visualization_only")}});

    if (m_simTime > 0.0 && simTimeSeconds + 0.001 < m_simTime)
    {
        resetCollisionTrackingState(false);
    }

    const bool antennaSnapshotChanged = (m_antennas != antennas);
    if (antennaSnapshotChanged)
    {
        m_antennas = antennas;
        m_antennaModel.updateRows(m_antennas);
        emit antennasChanged();
    }
    refreshUavDisplayState(simTimeSeconds);

    setRat(rat);
    setSimTime(simTimeSeconds);
    setHasData(!m_antennas.isEmpty() || !m_uavs.isEmpty());

    if (m_selectedUavId < 0 && !m_uavs.isEmpty())
    {
        const QVariantMap firstUav = m_uavs.constFirst().toMap();
        if (firstUav.contains(QStringLiteral("id")))
        {
            m_selectedUavId = firstUav.value(QStringLiteral("id")).toInt();
        }
    }

    refreshSelectedUav();
    setErrorString(QString());
}

void Ns3SimulationFeed::refreshUavDisplayState(double simTimeSeconds)
{
    QVariantList displayUavs;
    displayUavs.reserve(m_baseUavs.size());
    const double effectiveSimTime = simTimeSeconds >= 0.0 ? simTimeSeconds : m_simTime;

    for (const QVariant &entry : std::as_const(m_baseUavs))
    {
        QVariantMap row = entry.toMap();
        const int systemId = row.value(QStringLiteral("id")).toInt() + 1;
        if (row.value(QStringLiteral("label")).toString().isEmpty())
        {
            row.insert(QStringLiteral("label"),
                       QStringLiteral("UAV-%1").arg(systemId, 3, 10, QChar('0')));
        }
        if (m_manualControlActive && systemId == m_manualControlSystemId)
        {
            row = applyManualControlOverlayToRow(row,
                                                 m_manualRoll,
                                                 m_manualPitch,
                                                 m_manualYaw,
                                                 m_manualThrottle);
        }
        displayUavs.append(row);
    }

    refreshCollisionState(displayUavs, effectiveSimTime);
    m_uavs = displayUavs;
    m_uavModel.updateRows(m_uavs);
    emit uavsChanged();
    refreshAltitudeRange();
    refreshSelectedUav();
}

void Ns3SimulationFeed::refreshCollisionState(QVariantList &displayUavs, double simTimeSeconds)
{
    QVector<CollisionTrackedUav> trackedUavs;
    trackedUavs.reserve(displayUavs.size());

    QVector<CollisionNearestState> nearestStates(displayUavs.size());
    QVector<AvoidanceVector> avoidanceVectors(displayUavs.size());
    QHash<int, PreviousUavSample> currentSamples;

    for (int index = 0; index < displayUavs.size(); ++index)
    {
        QVariantMap row = displayUavs.at(index).toMap();
        row.insert(QStringLiteral("collisionSeverity"), QStringLiteral("SAFE"));
        row.insert(QStringLiteral("collisionPeerLabel"), QStringLiteral("--"));
        row.insert(QStringLiteral("collisionAutoAvoidanceActive"), false);
        row.insert(QStringLiteral("collisionMessage"), QStringLiteral("Clear"));
        row.insert(QStringLiteral("nearestUavDistanceMeters"), QVariant());
        row.insert(QStringLiteral("nearestUavVerticalDistanceMeters"), QVariant());
        row.insert(QStringLiteral("collisionHorizontalDistanceMeters"), QVariant());
        row.insert(QStringLiteral("collisionClosingSpeedMps"), QVariant());
        row.insert(QStringLiteral("collisionTimeToClosestSeconds"), QVariant());
        row.insert(QStringLiteral("collisionPredictedMinDistanceMeters"), QVariant());
        row.insert(QStringLiteral("collisionSafetyVolumeBreached"), false);
        row.insert(QStringLiteral("collisionAvoidanceMode"), QStringLiteral("NONE"));
        row.insert(QStringLiteral("collisionAvoidanceRoll"), 0);
        row.insert(QStringLiteral("collisionAvoidancePitch"), 0);
        row.insert(QStringLiteral("collisionAvoidanceThrottle"), 0);
        displayUavs[index] = row;

        CollisionTrackedUav tracked;
        tracked.rowIndex = index;
        tracked.id = row.value(QStringLiteral("id")).toInt();
        tracked.systemId = tracked.id >= 0 ? tracked.id + 1 : -1;
        tracked.label = row.value(QStringLiteral("label")).toString();
        tracked.latitude = row.value(QStringLiteral("latitude")).toDouble();
        tracked.longitude = row.value(QStringLiteral("longitude")).toDouble();
        tracked.altitudeMeters = row.value(QStringLiteral("altitudeMeters")).toDouble();
        tracked.headingDegrees = row.value(QStringLiteral("headingDegrees")).toDouble();
        tracked.valid = tracked.id >= 0
                        && std::isfinite(tracked.latitude)
                        && std::isfinite(tracked.longitude)
                        && std::isfinite(tracked.altitudeMeters);
        if (tracked.valid && tracked.systemId > 0)
        {
            const PreviousUavSample previousSample = m_previousUavSamples.value(tracked.systemId);
            if (previousSample.valid
                && simTimeSeconds > previousSample.simTimeSeconds + 0.001)
            {
                const double dt = simTimeSeconds - previousSample.simTimeSeconds;
                const double meanLatitude =
                    (tracked.latitude + previousSample.latitude) * 0.5;
                tracked.northVelocityMps =
                    (tracked.latitude - previousSample.latitude) * 111320.0 / dt;
                tracked.eastVelocityMps =
                    (tracked.longitude - previousSample.longitude)
                    * metersPerLongitudeDegree(meanLatitude) / dt;
                tracked.verticalVelocityMps =
                    (tracked.altitudeMeters - previousSample.altitudeMeters) / dt;
            }

            PreviousUavSample currentSample;
            currentSample.latitude = tracked.latitude;
            currentSample.longitude = tracked.longitude;
            currentSample.altitudeMeters = tracked.altitudeMeters;
            currentSample.simTimeSeconds = simTimeSeconds;
            currentSample.valid = true;
            currentSamples.insert(tracked.systemId, currentSample);
        }
        trackedUavs.append(tracked);
    }

    QVariantList collisionPairs;
    int warningCount = 0;
    int alertCount = 0;
    QString bannerSeverity;
    QString bannerText;
    double bannerDistanceMeters = std::numeric_limits<double>::infinity();
    QSet<int> alertSystems;
    QHash<QString, QString> currentPairSeverities;

    for (int left = 0; left < trackedUavs.size(); ++left)
    {
        if (!trackedUavs[left].valid)
        {
            continue;
        }

        for (int right = left + 1; right < trackedUavs.size(); ++right)
        {
            if (!trackedUavs[right].valid)
            {
                continue;
            }

            const double meanLatitude = (trackedUavs[left].latitude + trackedUavs[right].latitude) * 0.5;
            const double northMeters =
                (trackedUavs[right].latitude - trackedUavs[left].latitude) * 111320.0;
            const double eastMeters =
                (trackedUavs[right].longitude - trackedUavs[left].longitude)
                * metersPerLongitudeDegree(meanLatitude);
            const double verticalMeters =
                trackedUavs[right].altitudeMeters - trackedUavs[left].altitudeMeters;
            const double horizontalDistanceMeters =
                std::sqrt(northMeters * northMeters + eastMeters * eastMeters);
            const double distanceMeters =
                std::sqrt(horizontalDistanceMeters * horizontalDistanceMeters
                          + verticalMeters * verticalMeters);
            const double relativeNorthVelocity =
                trackedUavs[right].northVelocityMps - trackedUavs[left].northVelocityMps;
            const double relativeEastVelocity =
                trackedUavs[right].eastVelocityMps - trackedUavs[left].eastVelocityMps;
            const double relativeVerticalVelocity =
                trackedUavs[right].verticalVelocityMps - trackedUavs[left].verticalVelocityMps;
            const double dotPositionVelocity =
                northMeters * relativeNorthVelocity
                + eastMeters * relativeEastVelocity
                + verticalMeters * relativeVerticalVelocity;
            const double relativeVelocityMagnitudeSquared =
                relativeNorthVelocity * relativeNorthVelocity
                + relativeEastVelocity * relativeEastVelocity
                + relativeVerticalVelocity * relativeVerticalVelocity;
            const double closingSpeedMps =
                distanceMeters > 0.001 ? -(dotPositionVelocity / distanceMeters) : 0.0;
            double timeToClosestSeconds = -1.0;
            double predictedMinDistanceMeters = distanceMeters;
            if (relativeVelocityMagnitudeSquared > 1e-6)
            {
                const double candidateTime =
                    -dotPositionVelocity / relativeVelocityMagnitudeSquared;
                if (candidateTime > 0.0)
                {
                    timeToClosestSeconds = candidateTime;
                    const double projectedNorth =
                        northMeters + relativeNorthVelocity * candidateTime;
                    const double projectedEast =
                        eastMeters + relativeEastVelocity * candidateTime;
                    const double projectedVertical =
                        verticalMeters + relativeVerticalVelocity * candidateTime;
                    predictedMinDistanceMeters =
                        std::sqrt(projectedNorth * projectedNorth
                                  + projectedEast * projectedEast
                                  + projectedVertical * projectedVertical);
                }
            }

            const QString pairKey =
                pairKeyForSystems(trackedUavs[left].systemId, trackedUavs[right].systemId);
            const QString previousSeverity = m_previousPairSeverities.value(pairKey);
            const QString severity =
                collisionSeverityForPair(previousSeverity,
                                         distanceMeters,
                                         northMeters,
                                         eastMeters,
                                         verticalMeters,
                                         m_collisionPolicy.warningEnterMeters,
                                         m_collisionPolicy.warningExitMeters,
                                         m_collisionPolicy.alertEnterMeters,
                                         m_collisionPolicy.alertExitMeters,
                                         m_collisionPolicy.safetyAxisEnterMeters,
                                         m_collisionPolicy.safetyAxisExitMeters);
            if (severity.isEmpty())
            {
                continue;
            }

            currentPairSeverities.insert(pairKey, severity);
            if (severity == QStringLiteral("ALERT"))
            {
                alertCount += 1;
                alertSystems.insert(trackedUavs[left].systemId);
                alertSystems.insert(trackedUavs[right].systemId);
            }
            else
            {
                warningCount += 1;
            }

            QVariantMap pair;
            pair.insert(QStringLiteral("uavAId"), trackedUavs[left].id);
            pair.insert(QStringLiteral("uavALabel"), trackedUavs[left].label);
            pair.insert(QStringLiteral("uavBId"), trackedUavs[right].id);
            pair.insert(QStringLiteral("uavBLabel"), trackedUavs[right].label);
            pair.insert(QStringLiteral("pairKey"), pairKey);
            pair.insert(QStringLiteral("distanceMeters"), distanceMeters);
            pair.insert(QStringLiteral("horizontalDistanceMeters"), horizontalDistanceMeters);
            pair.insert(QStringLiteral("northMeters"), northMeters);
            pair.insert(QStringLiteral("eastMeters"), eastMeters);
            pair.insert(QStringLiteral("verticalMeters"), verticalMeters);
            pair.insert(QStringLiteral("closingSpeedMps"), closingSpeedMps);
            pair.insert(QStringLiteral("predictedMinDistanceMeters"), predictedMinDistanceMeters);
            pair.insert(QStringLiteral("safetyVolumeBreached"),
                        withinSafetyBox(northMeters,
                                        eastMeters,
                                        verticalMeters,
                                        m_collisionPolicy.safetyAxisEnterMeters));
            if (timeToClosestSeconds >= 0.0)
            {
                pair.insert(QStringLiteral("timeToClosestSeconds"), timeToClosestSeconds);
            }
            pair.insert(QStringLiteral("severity"), severity);
            collisionPairs.append(pair);

            auto updateNearestState = [&](int rowIndex,
                                          int peerId,
                                          const QString &peerLabel,
                                          double horizontalDelta,
                                          double northDelta,
                                          double eastDelta,
                                          double verticalDelta,
                                          double closingSpeed,
                                          double timeToClosest,
                                          double predictedMinDistance,
                                          bool safetyVolumeBreached,
                                          const QString &pairSeverity)
            {
                CollisionNearestState &nearest = nearestStates[rowIndex];
                const int currentSeverity = collisionSeverityRank(nearest.severity);
                const int pairSeverityRank = collisionSeverityRank(pairSeverity);
                if (pairSeverityRank > currentSeverity
                    || distanceMeters < nearest.distanceMeters)
                {
                    nearest.distanceMeters = distanceMeters;
                    nearest.horizontalDistanceMeters = horizontalDelta;
                    nearest.peerId = peerId;
                    nearest.peerLabel = peerLabel;
                    nearest.northMeters = northDelta;
                    nearest.eastMeters = eastDelta;
                    nearest.verticalMeters = verticalDelta;
                    nearest.closingSpeedMps = closingSpeed;
                    nearest.timeToClosestSeconds = timeToClosest;
                    nearest.predictedMinDistanceMeters = predictedMinDistance;
                    nearest.safetyVolumeBreached = safetyVolumeBreached;
                    nearest.severity = pairSeverity;
                }
            };

            updateNearestState(trackedUavs[left].rowIndex,
                               trackedUavs[right].id,
                               trackedUavs[right].label,
                               horizontalDistanceMeters,
                               northMeters,
                               eastMeters,
                               verticalMeters,
                               closingSpeedMps,
                               timeToClosestSeconds,
                               predictedMinDistanceMeters,
                               withinSafetyBox(northMeters,
                                               eastMeters,
                                               verticalMeters,
                                               m_collisionPolicy.safetyAxisEnterMeters),
                               severity);
            updateNearestState(trackedUavs[right].rowIndex,
                               trackedUavs[left].id,
                               trackedUavs[left].label,
                               horizontalDistanceMeters,
                               -northMeters,
                               -eastMeters,
                               -verticalMeters,
                               closingSpeedMps,
                               timeToClosestSeconds,
                               predictedMinDistanceMeters,
                               withinSafetyBox(northMeters,
                                               eastMeters,
                                               verticalMeters,
                                               m_collisionPolicy.safetyAxisEnterMeters),
                               severity);

            if (severity == QStringLiteral("ALERT"))
            {
                double planarDistance = horizontalDistanceMeters;
                double unitNorth = 0.0;
                double unitEast = 0.0;
                if (planarDistance > 0.001)
                {
                    unitNorth = northMeters / planarDistance;
                    unitEast = eastMeters / planarDistance;
                }
                else
                {
                    unitEast = trackedUavs[left].systemId <= trackedUavs[right].systemId ? 1.0 : -1.0;
                }

                avoidanceVectors[trackedUavs[left].rowIndex].north -= unitNorth;
                avoidanceVectors[trackedUavs[left].rowIndex].east -= unitEast;
                avoidanceVectors[trackedUavs[right].rowIndex].north += unitNorth;
                avoidanceVectors[trackedUavs[right].rowIndex].east += unitEast;

                double verticalUnit = 0.0;
                if (std::abs(verticalMeters) > 0.25)
                {
                    verticalUnit = verticalMeters > 0.0 ? 1.0 : -1.0;
                }
                else
                {
                    verticalUnit =
                        trackedUavs[left].systemId <= trackedUavs[right].systemId ? 1.0 : -1.0;
                }
                avoidanceVectors[trackedUavs[left].rowIndex].vertical -= verticalUnit;
                avoidanceVectors[trackedUavs[right].rowIndex].vertical += verticalUnit;
            }

            const int pairSeverityRank = collisionSeverityRank(severity);
            const int currentBannerRank = collisionSeverityRank(bannerSeverity);
            if (pairSeverityRank > currentBannerRank
                || (pairSeverityRank == currentBannerRank && distanceMeters < bannerDistanceMeters))
            {
                bannerSeverity = severity;
                bannerDistanceMeters = distanceMeters;
                bannerText = QStringLiteral("%1: %2 and %3 separation %4 m")
                                 .arg(severity)
                                 .arg(trackedUavs[left].label)
                                 .arg(trackedUavs[right].label)
                                 .arg(QString::number(distanceMeters, 'f', 1));
                if (timeToClosestSeconds >= 0.0)
                {
                    bannerText += QStringLiteral(" TTC %1 s")
                                      .arg(QString::number(timeToClosestSeconds, 'f', 1));
                }
            }

            if (previousSeverity != severity)
            {
                QVariantMap transitionFields;
                transitionFields.insert(QStringLiteral("status"),
                                        previousSeverity.isEmpty()
                                            ? QStringLiteral("entered")
                                            : QStringLiteral("changed"));
                transitionFields.insert(QStringLiteral("pair_key"), pairKey);
                transitionFields.insert(QStringLiteral("uav_a_id"), trackedUavs[left].systemId);
                transitionFields.insert(QStringLiteral("uav_a_label"), trackedUavs[left].label);
                transitionFields.insert(QStringLiteral("uav_b_id"), trackedUavs[right].systemId);
                transitionFields.insert(QStringLiteral("uav_b_label"), trackedUavs[right].label);
                transitionFields.insert(QStringLiteral("severity"), severity);
                if (!previousSeverity.isEmpty())
                {
                    transitionFields.insert(QStringLiteral("previous_severity"), previousSeverity);
                }
                transitionFields.insert(QStringLiteral("distance_m"), distanceMeters);
                transitionFields.insert(QStringLiteral("horizontal_distance_m"),
                                        horizontalDistanceMeters);
                transitionFields.insert(QStringLiteral("vertical_distance_m"),
                                        std::abs(verticalMeters));
                transitionFields.insert(QStringLiteral("closing_speed_mps"), closingSpeedMps);
                transitionFields.insert(QStringLiteral("predicted_min_distance_m"),
                                        predictedMinDistanceMeters);
                transitionFields.insert(QStringLiteral("safety_volume_breached"),
                                        withinSafetyBox(northMeters,
                                                        eastMeters,
                                                        verticalMeters,
                                                        m_collisionPolicy.safetyAxisEnterMeters));
                if (timeToClosestSeconds >= 0.0)
                {
                    transitionFields.insert(QStringLiteral("time_to_closest_s"),
                                            timeToClosestSeconds);
                }
                PublicationLogger::instance().logEvent(QStringLiteral("collision_state"),
                                                       transitionFields);
            }
        }
    }

    for (auto it = m_previousPairSeverities.constBegin();
         it != m_previousPairSeverities.constEnd();
         ++it)
    {
        if (currentPairSeverities.contains(it.key()))
        {
            continue;
        }

        PublicationLogger::instance().logEvent(
            QStringLiteral("collision_state"),
            {{QStringLiteral("status"), QStringLiteral("cleared")},
             {QStringLiteral("pair_key"), it.key()},
             {QStringLiteral("severity"), QStringLiteral("SAFE")},
             {QStringLiteral("previous_severity"), it.value()}});
    }
    m_previousPairSeverities = currentPairSeverities;
    m_previousUavSamples = currentSamples;

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    QVariantList avoidanceCommands;
    for (const CollisionTrackedUav &tracked : std::as_const(trackedUavs))
    {
        if (!tracked.valid)
        {
            continue;
        }

        QVariantMap row = displayUavs.at(tracked.rowIndex).toMap();
        const CollisionNearestState &nearest = nearestStates[tracked.rowIndex];
        if (std::isfinite(nearest.distanceMeters))
        {
            row.insert(QStringLiteral("collisionSeverity"), nearest.severity);
            row.insert(QStringLiteral("collisionPeerLabel"), nearest.peerLabel);
            row.insert(QStringLiteral("collisionPeerId"), nearest.peerId);
            row.insert(QStringLiteral("collisionMessage"),
                       QStringLiteral("%1 with %2 at %3 m")
                           .arg(nearest.severity)
                           .arg(nearest.peerLabel)
                           .arg(QString::number(nearest.distanceMeters, 'f', 1)));
            row.insert(QStringLiteral("nearestUavDistanceMeters"), nearest.distanceMeters);
            row.insert(QStringLiteral("collisionHorizontalDistanceMeters"),
                       nearest.horizontalDistanceMeters);
            row.insert(QStringLiteral("nearestUavVerticalDistanceMeters"),
                       std::abs(nearest.verticalMeters));
            row.insert(QStringLiteral("collisionClosingSpeedMps"), nearest.closingSpeedMps);
            row.insert(QStringLiteral("collisionSafetyVolumeBreached"),
                       nearest.safetyVolumeBreached);
            if (nearest.timeToClosestSeconds >= 0.0)
            {
                row.insert(QStringLiteral("collisionTimeToClosestSeconds"),
                           nearest.timeToClosestSeconds);
            }
            if (std::isfinite(nearest.predictedMinDistanceMeters))
            {
                row.insert(QStringLiteral("collisionPredictedMinDistanceMeters"),
                           nearest.predictedMinDistanceMeters);
            }
        }
        row.insert(QStringLiteral("collisionAutoAvoidanceActive"),
                   alertSystems.contains(tracked.systemId));

        if (!alertSystems.contains(tracked.systemId))
        {
            displayUavs[tracked.rowIndex] = row;
            continue;
        }

        const AvoidanceVector &avoidance = avoidanceVectors[tracked.rowIndex];
        const double vectorMagnitude =
            std::sqrt(avoidance.north * avoidance.north
                      + avoidance.east * avoidance.east
                      + avoidance.vertical * avoidance.vertical);
        if (vectorMagnitude < 0.001)
        {
            displayUavs[tracked.rowIndex] = row;
            continue;
        }

        const double headingRadians = qDegreesToRadians(tracked.headingDegrees);
        const double forward =
            avoidance.north * std::cos(headingRadians) + avoidance.east * std::sin(headingRadians);
        const double lateral =
            -avoidance.north * std::sin(headingRadians) + avoidance.east * std::cos(headingRadians);
        int pitch = clampManualAxis((forward / vectorMagnitude)
                                    * m_collisionPolicy.commandMagnitude);
        int roll = clampManualAxis((lateral / vectorMagnitude)
                                   * m_collisionPolicy.commandMagnitude);
        int throttle = clampManualAxis((avoidance.vertical / vectorMagnitude)
                                       * m_collisionPolicy.commandMagnitude);
        if (std::abs(pitch) < m_collisionPolicy.commandFloor)
        {
            pitch = pitch == 0 ? 0 : (pitch > 0 ? m_collisionPolicy.commandFloor
                                                : -m_collisionPolicy.commandFloor);
        }
        if (std::abs(roll) < m_collisionPolicy.commandFloor)
        {
            roll = roll == 0 ? 0 : (roll > 0 ? m_collisionPolicy.commandFloor
                                             : -m_collisionPolicy.commandFloor);
        }
        if (std::abs(throttle) < m_collisionPolicy.commandFloor)
        {
            throttle = throttle == 0 ? 0 : (throttle > 0 ? m_collisionPolicy.commandFloor
                                                         : -m_collisionPolicy.commandFloor);
        }

        const QString avoidanceMode = collisionActionNameForCommand(roll, pitch, throttle);
        row.insert(QStringLiteral("collisionAvoidanceMode"), avoidanceMode);
        row.insert(QStringLiteral("collisionAvoidanceRoll"), roll);
        row.insert(QStringLiteral("collisionAvoidancePitch"), pitch);
        row.insert(QStringLiteral("collisionAvoidanceThrottle"), throttle);
        displayUavs[tracked.rowIndex] = row;

        QVariantMap command;
        command.insert(QStringLiteral("systemId"), tracked.systemId);
        command.insert(QStringLiteral("uavId"), tracked.id);
        command.insert(QStringLiteral("label"), tracked.label);
        command.insert(QStringLiteral("roll"), roll);
        command.insert(QStringLiteral("pitch"), pitch);
        command.insert(QStringLiteral("yaw"), 0);
        command.insert(QStringLiteral("throttle"), throttle);
        command.insert(QStringLiteral("reason"), QStringLiteral("collision_alert"));
        command.insert(QStringLiteral("collisionAction"), avoidanceMode);
        command.insert(QStringLiteral("severity"), QStringLiteral("ALERT"));
        command.insert(QStringLiteral("sendMavlinkReport"),
                       m_collisionPolicy.sendMavlinkReport);
        command.insert(QStringLiteral("nearestPeerId"), nearest.peerId);
        command.insert(QStringLiteral("nearestPeerLabel"), nearest.peerLabel);
        command.insert(QStringLiteral("distanceMeters"), nearest.distanceMeters);
        command.insert(QStringLiteral("horizontalDistanceMeters"),
                       nearest.horizontalDistanceMeters);
        command.insert(QStringLiteral("verticalDistanceMeters"),
                       std::abs(nearest.verticalMeters));
        command.insert(QStringLiteral("closingSpeedMps"), nearest.closingSpeedMps);
        command.insert(QStringLiteral("predictedMinDistanceMeters"),
                       nearest.predictedMinDistanceMeters);
        command.insert(QStringLiteral("safetyVolumeBreached"), nearest.safetyVolumeBreached);
        if (nearest.timeToClosestSeconds >= 0.0)
        {
            command.insert(QStringLiteral("timeToClosestSeconds"),
                           nearest.timeToClosestSeconds);
        }

        const QVariantMap previous = m_lastAutoAvoidanceCommand.value(tracked.systemId);
        const bool changed = previous.value(QStringLiteral("roll")).toInt() != roll
                             || previous.value(QStringLiteral("pitch")).toInt() != pitch
                             || previous.value(QStringLiteral("yaw")).toInt() != 0
                             || previous.value(QStringLiteral("throttle")).toInt() != throttle;
        const qint64 lastSentMs =
            m_lastAutoAvoidanceCommandMs.value(tracked.systemId,
                                               nowMs - m_collisionPolicy.avoidanceIntervalMs - 1);
        if (changed || (nowMs - lastSentMs) >= m_collisionPolicy.avoidanceIntervalMs)
        {
            avoidanceCommands.append(command);
            m_lastAutoAvoidanceCommand.insert(tracked.systemId, command);
            m_lastAutoAvoidanceCommandMs.insert(tracked.systemId, nowMs);
            PublicationLogger::instance().logEvent(
                QStringLiteral("collision_avoidance"),
                {{QStringLiteral("status"), QStringLiteral("commanded")},
                 {QStringLiteral("uav_id"), tracked.systemId},
                 {QStringLiteral("label"), tracked.label},
                 {QStringLiteral("severity"), QStringLiteral("ALERT")},
                 {QStringLiteral("collision_action"), avoidanceMode},
                 {QStringLiteral("peer_uav_id"), nearest.peerId >= 0 ? nearest.peerId + 1 : 0},
                 {QStringLiteral("peer_label"), nearest.peerLabel},
                 {QStringLiteral("distance_m"), nearest.distanceMeters},
                 {QStringLiteral("horizontal_distance_m"),
                  nearest.horizontalDistanceMeters},
                 {QStringLiteral("vertical_distance_m"),
                  std::abs(nearest.verticalMeters)},
                 {QStringLiteral("closing_speed_mps"), nearest.closingSpeedMps},
                 {QStringLiteral("predicted_min_distance_m"),
                  nearest.predictedMinDistanceMeters},
                 {QStringLiteral("roll"), roll},
                 {QStringLiteral("pitch"), pitch},
                 {QStringLiteral("throttle"), throttle}});
        }
    }

    for (int systemId : std::as_const(m_activeAutoAvoidanceSystems))
    {
        if (alertSystems.contains(systemId))
        {
            continue;
        }

        const qint64 lastSentMs =
            m_lastAutoAvoidanceCommandMs.value(systemId,
                                               nowMs - m_collisionPolicy.avoidanceIntervalMs - 1);
        if ((nowMs - lastSentMs) < m_collisionPolicy.avoidanceIntervalMs)
        {
            continue;
        }

        QVariantMap clearCommand;
        clearCommand.insert(QStringLiteral("systemId"), systemId);
        clearCommand.insert(QStringLiteral("uavId"), systemId - 1);
        clearCommand.insert(QStringLiteral("label"),
                            QStringLiteral("UAV-%1").arg(systemId, 3, 10, QChar('0')));
        clearCommand.insert(QStringLiteral("roll"), 0);
        clearCommand.insert(QStringLiteral("pitch"), 0);
        clearCommand.insert(QStringLiteral("yaw"), 0);
        clearCommand.insert(QStringLiteral("throttle"), 0);
        clearCommand.insert(QStringLiteral("reason"), QStringLiteral("collision_clear"));
        clearCommand.insert(QStringLiteral("collisionAction"), QStringLiteral("NONE"));
        clearCommand.insert(QStringLiteral("severity"), QStringLiteral("SAFE"));
        clearCommand.insert(QStringLiteral("sendMavlinkReport"), false);
        avoidanceCommands.append(clearCommand);
        m_lastAutoAvoidanceCommand.insert(systemId, clearCommand);
        m_lastAutoAvoidanceCommandMs.insert(systemId, nowMs);
        PublicationLogger::instance().logEvent(
            QStringLiteral("collision_avoidance"),
            {{QStringLiteral("status"), QStringLiteral("cleared")},
             {QStringLiteral("uav_id"), systemId},
             {QStringLiteral("label"),
              QStringLiteral("UAV-%1").arg(systemId, 3, 10, QChar('0'))},
             {QStringLiteral("severity"), QStringLiteral("SAFE")},
             {QStringLiteral("collision_action"), QStringLiteral("NONE")}});
    }

    m_activeAutoAvoidanceSystems = alertSystems;

    const bool collisionStateChanged =
        m_collisionWarningCount != warningCount
        || m_collisionAlertCount != alertCount
        || m_collisionBannerSeverity != bannerSeverity
        || m_collisionBannerText != bannerText
        || m_collisionPairs != collisionPairs;

    m_collisionWarningCount = warningCount;
    m_collisionAlertCount = alertCount;
    m_collisionBannerSeverity = bannerSeverity;
    m_collisionBannerText = bannerText;
    m_collisionPairs = collisionPairs;

    if (collisionStateChanged)
    {
        emit collisionStatusChanged();
    }

    if (!avoidanceCommands.isEmpty())
    {
        emit collisionAvoidanceRequested(avoidanceCommands);
    }
}

void Ns3SimulationFeed::resetCollisionTrackingState(bool emitStatusSignal)
{
    const bool hadCollisionState = m_collisionWarningCount > 0
                                   || m_collisionAlertCount > 0
                                   || !m_collisionBannerText.isEmpty()
                                   || !m_collisionPairs.isEmpty();
    m_collisionWarningCount = 0;
    m_collisionAlertCount = 0;
    m_collisionBannerSeverity.clear();
    m_collisionBannerText.clear();
    m_collisionPairs.clear();
    m_previousUavSamples.clear();
    m_previousPairSeverities.clear();
    m_lastAutoAvoidanceCommand.clear();
    m_lastAutoAvoidanceCommandMs.clear();
    m_activeAutoAvoidanceSystems.clear();
    if (emitStatusSignal && hadCollisionState)
    {
        emit collisionStatusChanged();
    }
}

void Ns3SimulationFeed::logCollisionPolicyOnce()
{
    if (m_collisionPolicyLogged)
    {
        return;
    }

    PublicationLogger::instance().logEvent(
        QStringLiteral("collision_policy"),
        {{QStringLiteral("status"), QStringLiteral("configured")},
         {QStringLiteral("warning_enter_m"), m_collisionPolicy.warningEnterMeters},
         {QStringLiteral("warning_exit_m"), m_collisionPolicy.warningExitMeters},
         {QStringLiteral("alert_enter_m"), m_collisionPolicy.alertEnterMeters},
         {QStringLiteral("alert_exit_m"), m_collisionPolicy.alertExitMeters},
         {QStringLiteral("safety_axis_enter_m"),
          m_collisionPolicy.safetyAxisEnterMeters},
         {QStringLiteral("safety_axis_exit_m"),
          m_collisionPolicy.safetyAxisExitMeters},
         {QStringLiteral("command_interval_ms"),
          m_collisionPolicy.avoidanceIntervalMs},
         {QStringLiteral("command_magnitude"),
          m_collisionPolicy.commandMagnitude},
         {QStringLiteral("command_floor"),
          m_collisionPolicy.commandFloor},
         {QStringLiteral("send_mavlink_report"),
          m_collisionPolicy.sendMavlinkReport}});
    m_collisionPolicyLogged = true;
}

void Ns3SimulationFeed::refreshAltitudeRange()
{
    double minAltitude = std::numeric_limits<double>::infinity();
    double maxAltitude = -std::numeric_limits<double>::infinity();

    for (const QVariant &entry : std::as_const(m_uavs))
    {
        const QVariantMap row = entry.toMap();
        bool ok = false;
        const double altitude = row.value(QStringLiteral("altitudeMeters")).toDouble(&ok);
        if (!ok)
        {
            continue;
        }

        minAltitude = std::min(minAltitude, altitude);
        maxAltitude = std::max(maxAltitude, altitude);
    }

    if (!std::isfinite(minAltitude) || !std::isfinite(maxAltitude))
    {
        minAltitude = 0.0;
        maxAltitude = 0.0;
    }

    if (!qFuzzyCompare(m_minAltitudeMeters + 1.0, minAltitude + 1.0)
        || !qFuzzyCompare(m_maxAltitudeMeters + 1.0, maxAltitude + 1.0))
    {
        m_minAltitudeMeters = minAltitude;
        m_maxAltitudeMeters = maxAltitude;
        emit altitudeRangeChanged();
    }
}

void Ns3SimulationFeed::setListening(bool value)
{
    if (m_listening == value)
    {
        return;
    }
    m_listening = value;
    emit listeningChanged();
}

void Ns3SimulationFeed::setHasData(bool value)
{
    if (m_hasData == value)
    {
        return;
    }
    m_hasData = value;
    emit hasDataChanged();
}

void Ns3SimulationFeed::setRat(const QString &value)
{
    if (m_rat == value)
    {
        return;
    }
    m_rat = value;
    emit ratChanged();
}

void Ns3SimulationFeed::setSimTime(double value)
{
    if (qFuzzyCompare(m_simTime, value))
    {
        return;
    }
    m_simTime = value;
    emit simTimeChanged();
}

void Ns3SimulationFeed::setPort(quint16 value)
{
    if (m_port == value)
    {
        return;
    }
    m_port = value;
    emit portChanged();
}

void Ns3SimulationFeed::setErrorString(const QString &value)
{
    if (m_errorString == value)
    {
        return;
    }
    m_errorString = value;
    emit errorChanged();
}

void Ns3SimulationFeed::refreshSelectedUav()
{
    if (m_uavs.isEmpty())
    {
        setSelectedUavState(-1, QVariantMap());
        return;
    }

    if (m_selectedUavId < 0)
    {
        const QVariantMap firstUav = m_uavs.constFirst().toMap();
        setSelectedUavState(firstUav.value(QStringLiteral("id")).toInt(), firstUav);
        return;
    }

    for (const QVariant &entry : m_uavs)
    {
        const QVariantMap candidate = entry.toMap();
        if (candidate.contains(QStringLiteral("id"))
            && candidate.value(QStringLiteral("id")).toInt() == m_selectedUavId)
        {
            setSelectedUavState(m_selectedUavId, candidate);
            return;
        }
    }

    const QVariantMap firstUav = m_uavs.constFirst().toMap();
    setSelectedUavState(firstUav.value(QStringLiteral("id")).toInt(), firstUav);
}

void Ns3SimulationFeed::setSelectedUavState(int id, const QVariantMap &data)
{
    if (m_selectedUavId == id && m_selectedUav == data)
    {
        return;
    }

    m_selectedUavId = id;
    m_selectedUav = data;
    emit selectedUavChanged();
}
