#ifndef UDPGCS_H
#define UDPGCS_H

#include <QObject>
#include <QHostAddress>
#include <QUdpSocket>
#include <QDateTime>
#include <QString>

class udpgcs : public QObject
{
    Q_OBJECT
public:
    explicit udpgcs(QObject *parent = nullptr);
    ~udpgcs();

    struct ChannelConfig
    {
        QString name;
        QHostAddress bindAddress;
        quint16 bindPort = 0;
        QHostAddress remoteAddress;
        quint16 remotePort = 0;
    };

signals:
    void onModemChanged(QByteArray);
    void onPilotChanged(QByteArray);

    
public slots:
    void WriteModem(QByteArray Data);
    void WritePilot(QByteArray Data);
    void readyReadModem();
    void readyReadPilot();
    void bindHost();
    void WriteGimbal(QByteArray Data);
    void readyReadGimbal();

    
private:
    void configureChannels();
    bool bindSocket(QUdpSocket *socket, const ChannelConfig &channel);

    QUdpSocket *socketModem;
    QUdpSocket *socketPilot, *socketGimbal;
    ChannelConfig m_modemChannel;
    ChannelConfig m_pilotChannel;
    ChannelConfig m_gimbalChannel;
    QDateTime * time;
    qint64 msec2;
    qint64 msec;
    qint64 msec1;

    
};

#endif // UDPGCS_H
