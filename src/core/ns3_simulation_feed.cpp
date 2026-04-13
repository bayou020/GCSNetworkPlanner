#include "ns3_simulation_feed.h"
#include "publication_logger.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkDatagram>
#include <QVariantMap>
#include <QtMath>

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{

constexpr quint16 kDefaultNs3SimulationPort = 45454;

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

int defaultSnapshotUiUpdateMs()
{
    bool ok = false;
    const int configuredMs = qEnvironmentVariableIntValue("NP_GCS_NS3_UI_UPDATE_MS", &ok);
    if (!ok) {
        return 100;
    }
    return std::max(0, configuredMs);
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
    m_snapshotApplyTimer.setSingleShot(true);
    connect(&m_snapshotApplyTimer, &QTimer::timeout, this, &Ns3SimulationFeed::applyPendingSnapshot);
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
    bool receivedSnapshot = false;
    while (m_socket.hasPendingDatagrams())
    {
        const QNetworkDatagram datagram = m_socket.receiveDatagram();
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(datagram.data(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject())
        {
            setErrorString(tr("Invalid ns-3 simulation datagram: %1").arg(parseError.errorString()));
            PublicationLogger::instance().logError(
                QStringLiteral("invalid_ns3_simulation_datagram"),
                {{QStringLiteral("status"), QStringLiteral("parse_error")},
                 {QStringLiteral("note"), parseError.errorString()},
                 {QStringLiteral("datagram_bytes"), datagram.data().size()}});
            continue;
        }

        const QJsonObject root = document.object();
        const QString type = root.value(QStringLiteral("type")).toString();
        if (type == QStringLiteral("snapshot"))
        {
            m_pendingSnapshot = root;
            m_hasPendingSnapshot = true;
            receivedSnapshot = true;
            continue;
        }

        setErrorString(tr("Unsupported ns-3 simulation message type: %1").arg(type));
    }

    if (!receivedSnapshot || !m_hasPendingSnapshot) {
        return;
    }

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

    const QJsonObject snapshot = m_pendingSnapshot;
    m_hasPendingSnapshot = false;
    if (!m_snapshotApplyElapsed.isValid()) {
        m_snapshotApplyElapsed.start();
    } else {
        m_snapshotApplyElapsed.restart();
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

    m_antennas = antennas;
    m_antennaModel.updateRows(m_antennas);
    emit antennasChanged();
    refreshUavDisplayState();

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

void Ns3SimulationFeed::refreshUavDisplayState()
{
    QVariantList displayUavs;
    displayUavs.reserve(m_baseUavs.size());

    for (const QVariant &entry : std::as_const(m_baseUavs))
    {
        QVariantMap row = entry.toMap();
        const int systemId = row.value(QStringLiteral("id")).toInt() + 1;
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

    m_uavs = displayUavs;
    m_uavModel.updateRows(m_uavs);
    emit uavsChanged();
    refreshAltitudeRange();
    refreshSelectedUav();
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
