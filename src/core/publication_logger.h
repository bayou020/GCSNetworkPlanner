#ifndef PUBLICATION_LOGGER_H
#define PUBLICATION_LOGGER_H

#include <QElapsedTimer>
#include <QFile>
#include <QHash>
#include <QMutex>
#include <QString>
#include <QVariantMap>

class PublicationLogger
{
public:
    struct Context
    {
        QString scenarioId;
        QString runId;
        QString source;
        QString rat;
        QString securityProfile;
        QString syncMethod;
        QString syncNote;
        QString runNote;
        QString logRoot;
        double syncOffsetMs = 0.0;
        bool hasSyncOffset = false;
    };

    static PublicationLogger &instance();
    static Context defaultContext(const QString &source);

    void configure(const Context &context);

    bool isConfigured() const;
    const Context &context() const;
    QString logDirectory() const;
    qint64 monotonicMs() const;
    quint64 nextSequence(const QString &streamKey);
    QString nextCommandId(const QString &prefix = QStringLiteral("cmd"));
    void flush();
    void logEvent(const QString &eventType, const QVariantMap &fields = {});
    void logError(const QString &message, const QVariantMap &fields = {});

private:
    PublicationLogger() = default;

    void ensureConfigured();
    void flushLocked();
    void scheduleFlushLocked();
    void writeMetadataFileLocked();

    Context m_context;
    QFile m_eventFile;
    QString m_logDirectory;
    QElapsedTimer m_elapsed;
    mutable QMutex m_mutex;
    quint64 m_eventCounter = 0;
    QHash<QString, quint64> m_sequenceCounters;
    quint64 m_eventsSinceFlush = 0;
    qint64 m_lastFlushMs = 0;
    int m_flushIntervalMs = 250;
    int m_flushEventCount = 128;
    bool m_flushScheduled = false;
};

#endif // PUBLICATION_LOGGER_H
