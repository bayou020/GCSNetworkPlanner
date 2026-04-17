#ifndef NS3_SIMULATION_FEED_H
#define NS3_SIMULATION_FEED_H

#include <QAbstractListModel>
#include <QElapsedTimer>
#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QSet>
#include <QTimer>
#include <QUdpSocket>
#include <QVariantMap>
#include <QVariantList>
#include <QVector>

class Ns3SnapshotListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles
    {
        ModelDataRole = Qt::UserRole + 1
    };

    explicit Ns3SnapshotListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void clear();
    void updateRows(const QVariantList &items);

private:
    QString rowKey(const QVariantMap &row) const;

    QList<QVariantMap> m_rows;
};

class Ns3SimulationFeed : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool listening READ listening NOTIFY listeningChanged)
    Q_PROPERTY(bool hasData READ hasData NOTIFY hasDataChanged)
    Q_PROPERTY(QString rat READ rat NOTIFY ratChanged)
    Q_PROPERTY(double simTime READ simTime NOTIFY simTimeChanged)
    Q_PROPERTY(quint16 port READ port NOTIFY portChanged)
    Q_PROPERTY(QVariantList antennas READ antennas NOTIFY antennasChanged)
    Q_PROPERTY(QVariantList uavs READ uavs NOTIFY uavsChanged)
    Q_PROPERTY(QAbstractItemModel *antennaModel READ antennaModel CONSTANT)
    Q_PROPERTY(QAbstractItemModel *uavModel READ uavModel CONSTANT)
    Q_PROPERTY(int antennaCount READ antennaCount NOTIFY antennasChanged)
    Q_PROPERTY(int uavCount READ uavCount NOTIFY uavsChanged)
    Q_PROPERTY(double minAltitudeMeters READ minAltitudeMeters NOTIFY altitudeRangeChanged)
    Q_PROPERTY(double maxAltitudeMeters READ maxAltitudeMeters NOTIFY altitudeRangeChanged)
    Q_PROPERTY(int selectedUavId READ selectedUavId NOTIFY selectedUavChanged)
    Q_PROPERTY(bool hasSelectedUav READ hasSelectedUav NOTIFY selectedUavChanged)
    Q_PROPERTY(QVariantMap selectedUav READ selectedUav NOTIFY selectedUavChanged)
    Q_PROPERTY(int collisionWarningCount READ collisionWarningCount NOTIFY collisionStatusChanged)
    Q_PROPERTY(int collisionAlertCount READ collisionAlertCount NOTIFY collisionStatusChanged)
    Q_PROPERTY(QString collisionBannerSeverity READ collisionBannerSeverity NOTIFY collisionStatusChanged)
    Q_PROPERTY(QString collisionBannerText READ collisionBannerText NOTIFY collisionStatusChanged)
    Q_PROPERTY(QVariantList collisionPairs READ collisionPairs NOTIFY collisionStatusChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorChanged)

public:
    struct PendingChunkedSnapshot
    {
        quint32 messageId = 0;
        int totalSize = 0;
        int receivedChunks = 0;
        QVector<QByteArray> chunks;
    };

    explicit Ns3SimulationFeed(QObject *parent = nullptr);

    bool listening() const;
    bool hasData() const;
    QString rat() const;
    double simTime() const;
    quint16 port() const;
    QVariantList antennas() const;
    QVariantList uavs() const;
    QAbstractItemModel *antennaModel() const;
    QAbstractItemModel *uavModel() const;
    int antennaCount() const;
    int uavCount() const;
    double minAltitudeMeters() const;
    double maxAltitudeMeters() const;
    int selectedUavId() const;
    bool hasSelectedUav() const;
    QVariantMap selectedUav() const;
    int collisionWarningCount() const;
    int collisionAlertCount() const;
    QString collisionBannerSeverity() const;
    QString collisionBannerText() const;
    QVariantList collisionPairs() const;
    QString errorString() const;

    Q_INVOKABLE bool startListening(quint16 port = 0);
    Q_INVOKABLE void stopListening();
    Q_INVOKABLE void clear();
    Q_INVOKABLE void selectUavById(int id);
    Q_INVOKABLE void clearSelectedUav();
    Q_INVOKABLE void setManualControlOverlay(int systemId,
                                             int roll,
                                             int pitch,
                                             int yaw,
                                             int throttle,
                                             bool active);

signals:
    void listeningChanged();
    void hasDataChanged();
    void ratChanged();
    void simTimeChanged();
    void portChanged();
    void antennasChanged();
    void uavsChanged();
    void altitudeRangeChanged();
    void selectedUavChanged();
    void collisionStatusChanged();
    void collisionAvoidanceRequested(const QVariantList &commands);
    void errorChanged();

private slots:
    void processPendingDatagrams();
    void applyPendingSnapshot();

private:
    struct CollisionPolicy
    {
        double warningEnterMeters = 10.0;
        double warningExitMeters = 12.0;
        double alertEnterMeters = 6.0;
        double alertExitMeters = 8.0;
        double safetyAxisEnterMeters = 5.0;
        double safetyAxisExitMeters = 6.0;
        int avoidanceIntervalMs = 350;
        int commandMagnitude = 760;
        int commandFloor = 220;
        bool sendMavlinkReport = true;
    };

    struct PreviousUavSample
    {
        double latitude = 0.0;
        double longitude = 0.0;
        double altitudeMeters = 0.0;
        double simTimeSeconds = -1.0;
        bool valid = false;
    };

    void refreshUavDisplayState(double simTimeSeconds = -1.0);
    void refreshAltitudeRange();
    void refreshCollisionState(QVariantList &displayUavs, double simTimeSeconds);
    void applySnapshot(const QJsonObject &snapshot);
    void setListening(bool value);
    void setHasData(bool value);
    void setRat(const QString &value);
    void setSimTime(double value);
    void setPort(quint16 value);
    void setErrorString(const QString &value);
    void refreshSelectedUav();
    void setSelectedUavState(int id, const QVariantMap &data);
    void resetCollisionTrackingState(bool emitStatusSignal = false);
    void logCollisionPolicyOnce();

    QUdpSocket m_socket;
    bool m_listening = false;
    bool m_hasData = false;
    QString m_rat;
    double m_simTime = 0.0;
    quint16 m_port = 0;
    QVariantList m_antennas;
    QVariantList m_baseUavs;
    QVariantList m_uavs;
    Ns3SnapshotListModel m_antennaModel;
    Ns3SnapshotListModel m_uavModel;
    double m_minAltitudeMeters = 0.0;
    double m_maxAltitudeMeters = 0.0;
    int m_selectedUavId = -1;
    QVariantMap m_selectedUav;
    int m_collisionWarningCount = 0;
    int m_collisionAlertCount = 0;
    QString m_collisionBannerSeverity;
    QString m_collisionBannerText;
    QVariantList m_collisionPairs;
    QString m_errorString;
    int m_manualControlSystemId = -1;
    int m_manualRoll = 0;
    int m_manualPitch = 0;
    int m_manualYaw = 0;
    int m_manualThrottle = 0;
    bool m_manualControlActive = false;
    QElapsedTimer m_snapshotApplyElapsed;
    QTimer m_snapshotApplyTimer;
    QByteArray m_pendingSnapshotPayload;
    bool m_hasPendingSnapshot = false;
    int m_snapshotUiUpdateMs = 100;
    PendingChunkedSnapshot m_pendingChunkedSnapshot;
    CollisionPolicy m_collisionPolicy;
    bool m_collisionPolicyLogged = false;
    QHash<int, PreviousUavSample> m_previousUavSamples;
    QHash<QString, QString> m_previousPairSeverities;
    QHash<int, QVariantMap> m_lastAutoAvoidanceCommand;
    QHash<int, qint64> m_lastAutoAvoidanceCommandMs;
    QSet<int> m_activeAutoAvoidanceSystems;
};

#endif // NS3_SIMULATION_FEED_H
