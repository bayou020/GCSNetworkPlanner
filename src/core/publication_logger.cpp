#include "publication_logger.h"

#include <cmath>

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaType>
#include <QMutexLocker>

namespace
{

QString envString(const char *name, const QString &fallback = {})
{
    return qEnvironmentVariableIsSet(name) ? qEnvironmentVariable(name).trimmed() : fallback;
}

int envInt(const char *name, int fallback)
{
    bool ok = false;
    const int value = qEnvironmentVariableIntValue(name, &ok);
    return ok ? value : fallback;
}

QString sanitizedIdentifier(QString value, const QString &fallback)
{
    value = value.trimmed();
    if (value.isEmpty()) {
        return fallback;
    }

    for (QChar &character : value) {
        if (!character.isLetterOrNumber() && character != '-' && character != '_' && character != '.') {
            character = '-';
        }
    }

    return value;
}

QString utcTimestampString()
{
    return QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
}

bool shouldDefaultToFieldGroundTruth(const QString &eventType)
{
    return eventType == QLatin1String("command_tx")
           || eventType == QLatin1String("command_rx")
           || eventType == QLatin1String("command_ack")
           || eventType == QLatin1String("telemetry_rx")
           || eventType == QLatin1String("network_sample")
           || eventType == QLatin1String("packet_forward")
           || eventType == QLatin1String("battery_sample")
           || eventType == QLatin1String("sync_status");
}

QString defaultMetricOriginForEvent(const QString &eventType)
{
    if (eventType == QLatin1String("network_sample")) {
        return QStringLiteral("direct_measurement");
    }

    if (eventType == QLatin1String("command_tx")
        || eventType == QLatin1String("command_rx")
        || eventType == QLatin1String("command_ack")
        || eventType == QLatin1String("telemetry_rx")
        || eventType == QLatin1String("packet_forward")
        || eventType == QLatin1String("battery_sample")) {
        return QStringLiteral("direct_observation");
    }

    if (eventType == QLatin1String("sync_status")) {
        return QStringLiteral("runtime_metadata");
    }

    return {};
}

QJsonValue toJsonValue(const QVariant &value)
{
    if (!value.isValid()) {
        return QJsonValue::Null;
    }

    if (value.metaType().id() == QMetaType::Double) {
        const double numericValue = value.toDouble();
        if (!std::isfinite(numericValue)) {
            return QJsonValue::Null;
        }
    }

    return QJsonValue::fromVariant(value);
}

} // namespace

PublicationLogger &PublicationLogger::instance()
{
    static PublicationLogger logger;
    return logger;
}

PublicationLogger::Context PublicationLogger::defaultContext(const QString &source)
{
    Context context;
    context.source = sanitizedIdentifier(source, QStringLiteral("unknown-source"));
    context.scenarioId = sanitizedIdentifier(envString("NP_SCENARIO_ID"),
                                             QStringLiteral("unspecified-scenario"));
    context.runId = sanitizedIdentifier(
        envString("NP_RUN_ID"),
        QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd'T'HHmmss'Z'"))
            + QStringLiteral("-p")
            + QString::number(QCoreApplication::applicationPid()));
    context.rat = envString("NP_RAT");
    context.securityProfile = envString("NP_SECURITY_PROFILE");
    context.syncMethod = envString("NP_SYNC_METHOD", QStringLiteral("unspecified"));
    context.syncNote = envString("NP_SYNC_NOTE");
    context.runNote = envString("NP_RUN_NOTE");
    context.logRoot = envString("NP_LOG_ROOT", QDir::current().absoluteFilePath(QStringLiteral("logs")));

    bool syncOffsetOk = false;
    const double syncOffsetMs = envString("NP_SYNC_OFFSET_MS").toDouble(&syncOffsetOk);
    context.syncOffsetMs = syncOffsetMs;
    context.hasSyncOffset = syncOffsetOk;
    return context;
}

void PublicationLogger::configure(const Context &context)
{
    {
        QMutexLocker locker(&m_mutex);
        m_context = context;
        if (!m_elapsed.isValid()) {
            m_elapsed.start();
        }
        m_flushIntervalMs = std::max(0, envInt("NP_LOG_FLUSH_INTERVAL_MS", 250));
        m_flushEventCount = std::max(1, envInt("NP_LOG_FLUSH_EVENT_COUNT", 128));
        m_eventsSinceFlush = 0;
        m_lastFlushMs = m_elapsed.elapsed();

        m_logDirectory = QDir(context.logRoot)
                             .filePath(QStringLiteral("raw/%1/%2")
                                           .arg(context.scenarioId, context.runId));
        QDir directory;
        directory.mkpath(m_logDirectory);

        const QString eventPath =
            QDir(m_logDirectory).filePath(context.source + QStringLiteral("_events.jsonl"));
        if (m_eventFile.fileName() != eventPath) {
            if (m_eventFile.isOpen()) {
                m_eventFile.close();
            }
            m_eventFile.setFileName(eventPath);
            m_eventFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
        }

        writeMetadataFileLocked();
    }

    QVariantMap syncFields;
    syncFields.insert(QStringLiteral("status"), QStringLiteral("configured"));
    syncFields.insert(QStringLiteral("sync_method"), context.syncMethod);
    if (context.hasSyncOffset) {
        syncFields.insert(QStringLiteral("sync_offset_ms"), context.syncOffsetMs);
    }
    if (!context.syncNote.isEmpty()) {
        syncFields.insert(QStringLiteral("note"), context.syncNote);
    }
    logEvent(QStringLiteral("sync_status"), syncFields);
}

bool PublicationLogger::isConfigured() const
{
    QMutexLocker locker(&m_mutex);
    return !m_context.source.isEmpty() && m_eventFile.isOpen();
}

const PublicationLogger::Context &PublicationLogger::context() const
{
    return m_context;
}

QString PublicationLogger::logDirectory() const
{
    QMutexLocker locker(&m_mutex);
    return m_logDirectory;
}

qint64 PublicationLogger::monotonicMs() const
{
    QMutexLocker locker(&m_mutex);
    return m_elapsed.isValid() ? m_elapsed.elapsed() : 0;
}

quint64 PublicationLogger::nextSequence(const QString &streamKey)
{
    QMutexLocker locker(&m_mutex);
    ensureConfigured();
    return ++m_sequenceCounters[streamKey];
}

QString PublicationLogger::nextCommandId(const QString &prefix)
{
    const quint64 sequence = nextSequence(QStringLiteral("command-id"));
    return QStringLiteral("%1-%2-%3")
        .arg(m_context.source)
        .arg(prefix)
        .arg(sequence);
}

void PublicationLogger::logEvent(const QString &eventType, const QVariantMap &fields)
{
    QMutexLocker locker(&m_mutex);
    ensureConfigured();
    if (!m_eventFile.isOpen()) {
        return;
    }

    QJsonObject event;
    event.insert(QStringLiteral("schema_version"), 1);
    event.insert(QStringLiteral("scenario_id"), m_context.scenarioId);
    event.insert(QStringLiteral("run_id"), m_context.runId);
    event.insert(QStringLiteral("event_id"),
                 QStringLiteral("%1-%2")
                     .arg(m_context.source)
                     .arg(++m_eventCounter, 6, 10, QLatin1Char('0')));
    event.insert(QStringLiteral("source"), m_context.source);
    event.insert(QStringLiteral("event_type"), eventType);
    event.insert(QStringLiteral("timestamp_utc"), utcTimestampString());
    event.insert(QStringLiteral("timestamp_monotonic_ms"), m_elapsed.elapsed());
    if (!m_context.rat.isEmpty()) {
        event.insert(QStringLiteral("rat"), m_context.rat);
    }
    if (!m_context.securityProfile.isEmpty()) {
        event.insert(QStringLiteral("security_profile"), m_context.securityProfile);
    }
    if (!fields.contains(QStringLiteral("evidence_layer"))
        && shouldDefaultToFieldGroundTruth(eventType)) {
        event.insert(QStringLiteral("evidence_layer"), QStringLiteral("field_ground_truth"));
    }
    if (!fields.contains(QStringLiteral("metric_origin"))) {
        const QString metricOrigin = defaultMetricOriginForEvent(eventType);
        if (!metricOrigin.isEmpty()) {
            event.insert(QStringLiteral("metric_origin"), metricOrigin);
        }
    }

    for (auto it = fields.constBegin(); it != fields.constEnd(); ++it) {
        event.insert(it.key(), toJsonValue(it.value()));
    }

    m_eventFile.write(QJsonDocument(event).toJson(QJsonDocument::Compact));
    m_eventFile.write("\n");
    ++m_eventsSinceFlush;

    const qint64 nowMs = m_elapsed.elapsed();
    const bool flushDue = eventType == QLatin1String("error")
                          || m_eventsSinceFlush >= static_cast<quint64>(m_flushEventCount)
                          || m_flushIntervalMs == 0
                          || (nowMs - m_lastFlushMs) >= m_flushIntervalMs;
    if (flushDue) {
        m_eventFile.flush();
        m_eventsSinceFlush = 0;
        m_lastFlushMs = nowMs;
    }
}

void PublicationLogger::logError(const QString &message, const QVariantMap &fields)
{
    QVariantMap errorFields = fields;
    errorFields.insert(QStringLiteral("status"), QStringLiteral("error"));
    errorFields.insert(QStringLiteral("note"), message);
    logEvent(QStringLiteral("error"), errorFields);
}

void PublicationLogger::ensureConfigured()
{
    if (!m_elapsed.isValid()) {
        m_elapsed.start();
    }

    if (m_context.source.isEmpty()) {
        m_context = defaultContext(QStringLiteral("unknown-source"));
    }

    if (!m_logDirectory.isEmpty() && m_eventFile.isOpen()) {
        return;
    }

    m_logDirectory = QDir(m_context.logRoot)
                         .filePath(QStringLiteral("raw/%1/%2")
                                       .arg(m_context.scenarioId, m_context.runId));
    QDir directory;
    directory.mkpath(m_logDirectory);

    const QString eventPath =
        QDir(m_logDirectory).filePath(m_context.source + QStringLiteral("_events.jsonl"));
    m_eventFile.setFileName(eventPath);
    m_eventFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
    writeMetadataFileLocked();
}

void PublicationLogger::writeMetadataFileLocked()
{
    if (m_logDirectory.isEmpty()) {
        return;
    }

    QJsonObject metadata;
    metadata.insert(QStringLiteral("schema_name"), QStringLiteral("networkplanner_publication_events"));
    metadata.insert(QStringLiteral("schema_version"), 1);
    metadata.insert(QStringLiteral("scenario_id"), m_context.scenarioId);
    metadata.insert(QStringLiteral("run_id"), m_context.runId);
    metadata.insert(QStringLiteral("source"), m_context.source);
    metadata.insert(QStringLiteral("event_log"),
                    QDir(m_logDirectory).filePath(m_context.source + QStringLiteral("_events.jsonl")));
    metadata.insert(QStringLiteral("created_at_utc"), utcTimestampString());
    if (!m_context.rat.isEmpty()) {
        metadata.insert(QStringLiteral("rat"), m_context.rat);
    }
    if (!m_context.securityProfile.isEmpty()) {
        metadata.insert(QStringLiteral("security_profile"), m_context.securityProfile);
    }
    metadata.insert(QStringLiteral("sync_method"), m_context.syncMethod);
    if (m_context.hasSyncOffset) {
        metadata.insert(QStringLiteral("sync_offset_ms"), m_context.syncOffsetMs);
    }
    if (!m_context.syncNote.isEmpty()) {
        metadata.insert(QStringLiteral("sync_note"), m_context.syncNote);
    }
    if (!m_context.runNote.isEmpty()) {
        metadata.insert(QStringLiteral("run_note"), m_context.runNote);
    }

    QFile metadataFile(
        QDir(m_logDirectory).filePath(m_context.source + QStringLiteral("_metadata.json")));
    if (!metadataFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        return;
    }

    metadataFile.write(QJsonDocument(metadata).toJson(QJsonDocument::Indented));
    metadataFile.close();
}
