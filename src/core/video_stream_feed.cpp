#include "video_stream_feed.h"

#include <algorithm>
#include <cstring>
#include <utility>

#include <QDataStream>
#include <QDateTime>
#include <QNetworkDatagram>
#include <QTimer>

namespace
{

constexpr char kPacketMagicV1[] = {'N', 'P', 'V', '1'};
constexpr char kPacketMagicV2[] = {'N', 'P', 'V', '2'};
constexpr quint16 kDefaultVideoPort = 5600;

} // namespace

VideoStreamFeed::VideoStreamFeed(QObject *parent)
    : QObject(parent)
{
    connect(&m_socket, &QUdpSocket::readyRead, this, &VideoStreamFeed::processPendingDatagrams);

    auto *ageTimer = new QTimer(this);
    ageTimer->setInterval(250);
    connect(ageTimer, &QTimer::timeout, this, &VideoStreamFeed::refreshSelectedFrameAge);
    ageTimer->start();
}

bool VideoStreamFeed::listening() const
{
    return m_listening;
}

quint16 VideoStreamFeed::port() const
{
    return m_port;
}

int VideoStreamFeed::selectedUavId() const
{
    return m_selectedUavId;
}

bool VideoStreamFeed::hasSelectedFrame() const
{
    return m_selectedUavId >= 0 && m_states.contains(m_selectedUavId)
           && !m_states.value(m_selectedUavId).jpegFrame.isEmpty();
}

QString VideoStreamFeed::selectedFrameUrl() const
{
    if (!hasSelectedFrame())
    {
        return QString();
    }
    return m_states.value(m_selectedUavId).frameUrl;
}

QString VideoStreamFeed::selectedResolution() const
{
    if (!hasSelectedFrame())
    {
        return QStringLiteral("--");
    }

    const UavVideoState &state = m_states.value(m_selectedUavId);
    return QStringLiteral("%1x%2").arg(state.width).arg(state.height);
}

double VideoStreamFeed::selectedReceiveFps() const
{
    if (!hasSelectedFrame())
    {
        return 0.0;
    }

    const UavVideoState &state = m_states.value(m_selectedUavId);
    if (state.receiveTimes.size() < 2)
    {
        return 0.0;
    }

    const qint64 spanMs = state.receiveTimes.back() - state.receiveTimes.front();
    if (spanMs <= 0)
    {
        return 0.0;
    }

    return static_cast<double>(state.receiveTimes.size() - 1) * 1000.0 / static_cast<double>(spanMs);
}

int VideoStreamFeed::selectedReceivedFrames() const
{
    return hasSelectedFrame() ? m_states.value(m_selectedUavId).receivedFrames : 0;
}

int VideoStreamFeed::selectedDroppedFrames() const
{
    return hasSelectedFrame() ? m_states.value(m_selectedUavId).droppedFrames : 0;
}

int VideoStreamFeed::selectedFrameAgeMs() const
{
    if (!hasSelectedFrame())
    {
        return -1;
    }

    const qint64 age = QDateTime::currentMSecsSinceEpoch() - m_states.value(m_selectedUavId).lastReceivedMs;
    return static_cast<int>(std::max<qint64>(0, age));
}

QString VideoStreamFeed::errorString() const
{
    return m_errorString;
}

bool VideoStreamFeed::startListening(quint16 port)
{
    const quint16 targetPort = port == 0 ? defaultPortFromEnvironment() : port;

    if (m_socket.isOpen())
    {
        m_socket.close();
    }

    if (!m_socket.bind(QHostAddress::LocalHost,
                       targetPort,
                       QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint))
    {
        setListening(false);
        setPort(0);
        setErrorString(tr("Failed to bind video stream feed on 127.0.0.1:%1: %2")
                           .arg(targetPort)
                           .arg(m_socket.errorString()));
        return false;
    }

    setPort(targetPort);
    setListening(true);
    setErrorString(QString());
    qInfo() << "VideoStreamFeed listening on 127.0.0.1:" << targetPort;
    return true;
}

void VideoStreamFeed::stopListening()
{
    if (m_socket.isOpen())
    {
        m_socket.close();
    }
    setListening(false);
    setPort(0);
}

void VideoStreamFeed::setSelectedUavId(int id)
{
    if (m_selectedUavId == id)
    {
        return;
    }

    m_selectedUavId = id;
    emit selectedUavIdChanged();
    updateSelectedFrameCache();
}

void VideoStreamFeed::processPendingDatagrams()
{
    const auto applyCompleteFrame =
        [this](int uavId, int width, int height, quint32 sequence, const QByteArray &jpegFrame)
    {
        UavVideoState &state = m_states[uavId];
        if (state.lastSequence != 0 && sequence > state.lastSequence + 1)
        {
            state.droppedFrames += static_cast<int>(sequence - state.lastSequence - 1);
        }

        state.lastSequence = sequence;
        state.receivedFrames += 1;
        state.width = width;
        state.height = height;
        state.lastReceivedMs = QDateTime::currentMSecsSinceEpoch();
        state.jpegFrame = jpegFrame;
        state.receiveTimes.enqueue(state.lastReceivedMs);
        while (state.receiveTimes.size() > 24)
        {
            state.receiveTimes.dequeue();
        }

        state.pendingFrame = UavVideoState::PendingFrame();

        if (uavId == m_selectedUavId)
        {
            refreshFrameUrl(state);
        }

        if (state.receivedFrames == 1)
        {
            qInfo() << "VideoStreamFeed received first frame for UAV" << uavId;
        }

        if (uavId == m_selectedUavId)
        {
            emit selectedFrameChanged();
        }
    };

    while (m_socket.hasPendingDatagrams())
    {
        const QNetworkDatagram datagram = m_socket.receiveDatagram();
        QDataStream stream(datagram.data());
        stream.setByteOrder(QDataStream::BigEndian);

        char magic[4] = {};
        if (stream.readRawData(magic, 4) != 4)
        {
            setErrorString(tr("Ignoring malformed simulated video datagram."));
            continue;
        }

        const bool isPacketV1 = std::memcmp(magic, kPacketMagicV1, 4) == 0;
        const bool isPacketV2 = std::memcmp(magic, kPacketMagicV2, 4) == 0;
        if (!isPacketV1 && !isPacketV2)
        {
            setErrorString(tr("Ignoring malformed simulated video datagram."));
            continue;
        }

        quint16 uavId = 0;
        quint16 width = 0;
        quint16 height = 0;
        quint32 sequence = 0;
        quint32 timestampMs = 0;
        stream >> uavId >> width >> height >> sequence >> timestampMs;
        Q_UNUSED(timestampMs);

        if (isPacketV1)
        {
            quint32 payloadSize = 0;
            stream >> payloadSize;

            if (payloadSize == 0 || payloadSize > 60000)
            {
                setErrorString(tr("Ignoring simulated video datagram with an invalid payload size."));
                continue;
            }

            QByteArray jpegFrame;
            jpegFrame.resize(static_cast<int>(payloadSize));
            if (stream.readRawData(jpegFrame.data(), static_cast<int>(payloadSize))
                != static_cast<int>(payloadSize))
            {
                setErrorString(tr("Ignoring truncated simulated video datagram."));
                continue;
            }

            applyCompleteFrame(static_cast<int>(uavId),
                               static_cast<int>(width),
                               static_cast<int>(height),
                               sequence,
                               jpegFrame);
            continue;
        }

        quint32 totalSize = 0;
        quint16 chunkIndex = 0;
        quint16 chunkCount = 0;
        quint32 chunkSize = 0;
        stream >> totalSize >> chunkIndex >> chunkCount >> chunkSize;

        if (totalSize == 0 || totalSize > 2 * 1024 * 1024 || chunkCount == 0
            || chunkIndex >= chunkCount || chunkSize == 0 || chunkSize > 60000)
        {
            setErrorString(tr("Ignoring simulated video datagram with invalid chunk metadata."));
            continue;
        }

        QByteArray chunkData;
        chunkData.resize(static_cast<int>(chunkSize));
        if (stream.readRawData(chunkData.data(), static_cast<int>(chunkSize))
            != static_cast<int>(chunkSize))
        {
            setErrorString(tr("Ignoring truncated simulated video datagram."));
            continue;
        }

        UavVideoState &state = m_states[static_cast<int>(uavId)];
        UavVideoState::PendingFrame &pending = state.pendingFrame;
        if (pending.sequence != sequence || pending.totalSize != static_cast<int>(totalSize)
            || pending.chunks.size() != chunkCount)
        {
            pending = UavVideoState::PendingFrame();
            pending.sequence = sequence;
            pending.width = static_cast<int>(width);
            pending.height = static_cast<int>(height);
            pending.totalSize = static_cast<int>(totalSize);
            pending.chunks.resize(chunkCount);
        }

        if (pending.chunks[chunkIndex].isEmpty())
        {
            pending.receivedChunks += 1;
        }
        pending.chunks[chunkIndex] = chunkData;

        if (pending.receivedChunks < pending.chunks.size())
        {
            continue;
        }

        QByteArray jpegFrame;
        jpegFrame.reserve(pending.totalSize);
        bool complete = true;
        for (const QByteArray &chunk : std::as_const(pending.chunks))
        {
            if (chunk.isEmpty())
            {
                complete = false;
                break;
            }
            jpegFrame.append(chunk);
        }

        if (!complete || jpegFrame.size() != pending.totalSize)
        {
            state.pendingFrame = UavVideoState::PendingFrame();
            setErrorString(tr("Ignoring incomplete simulated video frame."));
            continue;
        }

        applyCompleteFrame(static_cast<int>(uavId),
                           pending.width,
                           pending.height,
                           sequence,
                           jpegFrame);
    }
}

void VideoStreamFeed::refreshSelectedFrameAge()
{
    if (hasSelectedFrame())
    {
        emit selectedFrameChanged();
    }
}

void VideoStreamFeed::setListening(bool value)
{
    if (m_listening == value)
    {
        return;
    }

    m_listening = value;
    emit listeningChanged();
}

void VideoStreamFeed::setPort(quint16 value)
{
    if (m_port == value)
    {
        return;
    }

    m_port = value;
    emit portChanged();
}

void VideoStreamFeed::setErrorString(const QString &value)
{
    if (m_errorString == value)
    {
        return;
    }

    m_errorString = value;
    emit errorChanged();
}

void VideoStreamFeed::updateSelectedFrameCache()
{
    if (m_selectedUavId >= 0)
    {
        auto it = m_states.find(m_selectedUavId);
        if (it != m_states.end())
        {
            refreshFrameUrl(it.value());
        }
    }
    emit selectedFrameChanged();
}

void VideoStreamFeed::refreshFrameUrl(UavVideoState &state)
{
    if (state.jpegFrame.isEmpty())
    {
        state.frameUrl.clear();
        return;
    }

    state.frameUrl =
        QStringLiteral("data:image/jpeg;base64,%1").arg(QString::fromLatin1(state.jpegFrame.toBase64()));
}

quint16 VideoStreamFeed::defaultPortFromEnvironment()
{
    bool ok = false;
    int configured = qEnvironmentVariableIntValue("NPGCS_VIDEO_PORT", &ok);
    if (!ok)
    {
        configured = qEnvironmentVariableIntValue("NPVIDEO_PORT", &ok);
    }
    if (!ok || configured <= 0 || configured > 65535)
    {
        return kDefaultVideoPort;
    }
    return static_cast<quint16>(configured);
}
