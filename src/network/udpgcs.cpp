#include "udpgcs.h"

#include <QDebug>

namespace
{

quint16 envPort(const QString &name, quint16 fallback)
{
    bool ok = false;
    const int value = qEnvironmentVariableIntValue(name.toUtf8().constData(), &ok);
    if (!ok || value <= 0 || value > 65535) {
        return fallback;
    }
    return static_cast<quint16>(value);
}

QHostAddress envAddress(const QString &name, const QHostAddress &fallback)
{
    if (!qEnvironmentVariableIsSet(name.toUtf8().constData())) {
        return fallback;
    }

    const QHostAddress address(qEnvironmentVariable(name.toUtf8().constData()).trimmed());
    return address.isNull() ? fallback : address;
}

bool hasAnyOverride(const QString &prefix)
{
    const QStringList names{
        prefix + "_HOST",
        prefix + "_BIND_HOST",
        prefix + "_REMOTE_HOST",
        prefix + "_BIND_PORT",
        prefix + "_REMOTE_PORT"
    };

    for (const QString &name : names) {
        if (qEnvironmentVariableIsSet(name.toUtf8().constData())) {
            return true;
        }
    }
    return false;
}

void applyDirectOverrides(udpgcs::ChannelConfig &channel, const QString &prefix)
{
    channel.bindAddress = envAddress(prefix + "_HOST", channel.bindAddress);
    channel.remoteAddress = envAddress(prefix + "_HOST", channel.remoteAddress);

    channel.bindAddress = envAddress(prefix + "_BIND_HOST", channel.bindAddress);
    channel.remoteAddress = envAddress(prefix + "_REMOTE_HOST", channel.remoteAddress);

    channel.bindPort = envPort(prefix + "_BIND_PORT", channel.bindPort);
    channel.remotePort = envPort(prefix + "_REMOTE_PORT", channel.remotePort);
}

void deriveFromRpiChannel(udpgcs::ChannelConfig &channel, const QString &rpiPrefix)
{
    channel.bindAddress = envAddress(rpiPrefix + "_HOST", channel.bindAddress);
    channel.remoteAddress = envAddress(rpiPrefix + "_HOST", channel.remoteAddress);

    channel.bindAddress = envAddress(rpiPrefix + "_REMOTE_HOST", channel.bindAddress);
    channel.remoteAddress = envAddress(rpiPrefix + "_BIND_HOST", channel.remoteAddress);

    channel.bindPort = envPort(rpiPrefix + "_REMOTE_PORT", channel.bindPort);
    channel.remotePort = envPort(rpiPrefix + "_BIND_PORT", channel.remotePort);
}

} // namespace

udpgcs::udpgcs(QObject *parent)
    : QObject(parent)
{
    socketModem = new QUdpSocket(this);
    socketPilot = new QUdpSocket(this);
    socketGimbal = new QUdpSocket(this);

    connect(socketModem, &QUdpSocket::readyRead, this, &udpgcs::readyReadModem);
    connect(socketPilot, &QUdpSocket::readyRead, this, &udpgcs::readyReadPilot);
    connect(socketGimbal, &QUdpSocket::readyRead, this, &udpgcs::readyReadGimbal);

    configureChannels();
}

void udpgcs::configureChannels()
{
    m_modemChannel = {
        QStringLiteral("modem"),
        QHostAddress(QStringLiteral("10.8.0.62")), 14581,
        QHostAddress(QStringLiteral("10.8.0.66")), 14582
    };
    m_pilotChannel = {
        QStringLiteral("pilot"),
        QHostAddress(QStringLiteral("10.8.0.62")), 14551,
        QHostAddress(QStringLiteral("10.8.0.66")), 14552
    };
    m_gimbalChannel = {
        QStringLiteral("gimbal"),
        QHostAddress(QStringLiteral("10.8.0.62")), 14591,
        QHostAddress(QStringLiteral("10.8.0.66")), 14592
    };

    applyDirectOverrides(m_modemChannel, QStringLiteral("NPGCS_MODEM"));
    applyDirectOverrides(m_pilotChannel, QStringLiteral("NPGCS_PILOT"));
    applyDirectOverrides(m_gimbalChannel, QStringLiteral("NPGCS_GIMBAL"));

    if (!hasAnyOverride(QStringLiteral("NPGCS_MODEM"))) {
        deriveFromRpiChannel(m_modemChannel, QStringLiteral("NPRPI_MODEM"));
    }
    if (!hasAnyOverride(QStringLiteral("NPGCS_PILOT"))) {
        deriveFromRpiChannel(m_pilotChannel, QStringLiteral("NPRPI_PILOT"));
    }
    if (!hasAnyOverride(QStringLiteral("NPGCS_GIMBAL"))) {
        deriveFromRpiChannel(m_gimbalChannel, QStringLiteral("NPRPI_GIMBAL"));
    }

    const auto logChannel = [](const ChannelConfig &channel) {
        qInfo().noquote()
            << QString("GCS %1 UDP channel bind %2:%3 -> %4:%5")
                   .arg(channel.name,
                        channel.bindAddress.toString())
                   .arg(channel.bindPort)
                   .arg(channel.remoteAddress.toString())
                   .arg(channel.remotePort);
    };

    logChannel(m_modemChannel);
    logChannel(m_pilotChannel);
    logChannel(m_gimbalChannel);
}

bool udpgcs::bindSocket(QUdpSocket *socket, const ChannelConfig &channel)
{
    if (socket->state() == QAbstractSocket::BoundState
        && socket->localAddress() == channel.bindAddress
        && socket->localPort() == channel.bindPort) {
        return true;
    }

    if (socket->state() == QAbstractSocket::BoundState) {
        socket->close();
    }

    if (!socket->bind(channel.bindAddress, channel.bindPort,
                      QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
        qWarning().noquote()
            << QString("Failed to bind GCS %1 UDP socket on %2:%3: %4")
                   .arg(channel.name,
                        channel.bindAddress.toString())
                   .arg(channel.bindPort)
                   .arg(socket->errorString());
        return false;
    }

    return true;
}

void udpgcs::WriteModem(QByteArray Data)
{
    socketModem->writeDatagram(Data, m_modemChannel.remoteAddress, m_modemChannel.remotePort);
    qDebug() << "Message sent: " << Data;
}

void udpgcs::WritePilot(QByteArray Data)
{
    socketPilot->writeDatagram(Data, m_pilotChannel.remoteAddress, m_pilotChannel.remotePort);
}

void udpgcs::WriteGimbal(QByteArray Data)
{
    socketGimbal->writeDatagram(Data, m_gimbalChannel.remoteAddress, m_gimbalChannel.remotePort);
}

void udpgcs::readyReadModem()
{
    QByteArray buffer;
    buffer.resize(socketModem->pendingDatagramSize());

    QHostAddress sender;
    quint16 senderPort;
    socketModem->readDatagram(buffer.data(), buffer.size(), &sender, &senderPort);
    emit onModemChanged(buffer);
}

void udpgcs::readyReadPilot()
{
    QByteArray buffer;
    buffer.resize(socketPilot->pendingDatagramSize());

    QHostAddress sender;
    quint16 senderPort;
    socketPilot->readDatagram(buffer.data(), buffer.size(), &sender, &senderPort);
    emit onPilotChanged(buffer);
}

void udpgcs::readyReadGimbal()
{
    QByteArray buffer;
    buffer.resize(socketGimbal->pendingDatagramSize());

    QHostAddress sender;
    quint16 senderPort;
    socketGimbal->readDatagram(buffer.data(), buffer.size(), &sender, &senderPort);
}

void udpgcs::bindHost()
{
    bindSocket(socketModem, m_modemChannel);
    bindSocket(socketPilot, m_pilotChannel);
    bindSocket(socketGimbal, m_gimbalChannel);
}

udpgcs::~udpgcs()
{
}
