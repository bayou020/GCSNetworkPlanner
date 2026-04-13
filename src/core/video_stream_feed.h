#ifndef VIDEO_STREAM_FEED_H
#define VIDEO_STREAM_FEED_H

#include <QObject>
#include <QHash>
#include <QQueue>
#include <QUdpSocket>

class VideoStreamFeed : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool listening READ listening NOTIFY listeningChanged)
    Q_PROPERTY(quint16 port READ port NOTIFY portChanged)
    Q_PROPERTY(int selectedUavId READ selectedUavId WRITE setSelectedUavId NOTIFY selectedUavIdChanged)
    Q_PROPERTY(bool hasSelectedFrame READ hasSelectedFrame NOTIFY selectedFrameChanged)
    Q_PROPERTY(QString selectedFrameUrl READ selectedFrameUrl NOTIFY selectedFrameChanged)
    Q_PROPERTY(QString selectedResolution READ selectedResolution NOTIFY selectedFrameChanged)
    Q_PROPERTY(double selectedReceiveFps READ selectedReceiveFps NOTIFY selectedFrameChanged)
    Q_PROPERTY(int selectedReceivedFrames READ selectedReceivedFrames NOTIFY selectedFrameChanged)
    Q_PROPERTY(int selectedDroppedFrames READ selectedDroppedFrames NOTIFY selectedFrameChanged)
    Q_PROPERTY(int selectedFrameAgeMs READ selectedFrameAgeMs NOTIFY selectedFrameChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorChanged)

public:
    explicit VideoStreamFeed(QObject *parent = nullptr);

    bool listening() const;
    quint16 port() const;
    int selectedUavId() const;
    bool hasSelectedFrame() const;
    QString selectedFrameUrl() const;
    QString selectedResolution() const;
    double selectedReceiveFps() const;
    int selectedReceivedFrames() const;
    int selectedDroppedFrames() const;
    int selectedFrameAgeMs() const;
    QString errorString() const;

    Q_INVOKABLE bool startListening(quint16 port = 0);
    Q_INVOKABLE void stopListening();

public slots:
    void setSelectedUavId(int id);

signals:
    void listeningChanged();
    void portChanged();
    void selectedUavIdChanged();
    void selectedFrameChanged();
    void errorChanged();

private slots:
    void processPendingDatagrams();
    void refreshSelectedFrameAge();

private:
    struct UavVideoState
    {
        QByteArray jpegFrame;
        QString frameUrl;
        quint32 lastSequence = 0;
        int receivedFrames = 0;
        int droppedFrames = 0;
        int width = 0;
        int height = 0;
        qint64 lastReceivedMs = 0;
        QQueue<qint64> receiveTimes;
    };

    void setListening(bool value);
    void setPort(quint16 value);
    void setErrorString(const QString &value);
    void updateSelectedFrameCache();
    static void refreshFrameUrl(UavVideoState &state);
    static quint16 defaultPortFromEnvironment();

    QUdpSocket m_socket;
    bool m_listening = false;
    quint16 m_port = 0;
    int m_selectedUavId = -1;
    QHash<int, UavVideoState> m_states;
    QString m_errorString;
};

#endif
