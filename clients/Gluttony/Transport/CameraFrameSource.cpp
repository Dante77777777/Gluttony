#include "CameraFrameSource.h"

#include <QDateTime>

CameraFrameSource::CameraFrameSource(QObject* parent)
    : QObject(parent)
    , receiver_(new CameraStreamReceiver(this))
{
    qRegisterMetaType<DecodedCameraFrame>("DecodedCameraFrame");

    connect(receiver_, &CameraStreamReceiver::sigServerStarted, this, &CameraFrameSource::sigServerStarted);

    connect(receiver_, &CameraStreamReceiver::sigServerStopped, this, &CameraFrameSource::sigServerStopped);

    connect(receiver_, &CameraStreamReceiver::sigClientConnected, this, &CameraFrameSource::sigClientConnected);

    connect(receiver_, &CameraStreamReceiver::sigClientDisconnected, this, &CameraFrameSource::sigClientDisconnected);

    connect(receiver_, &CameraStreamReceiver::sigClientRejected, this, &CameraFrameSource::sigClientRejected);

    connect(receiver_, &CameraStreamReceiver::sigFrameReceived, this, &CameraFrameSource::onPacketReceived);

    connect(receiver_, &CameraStreamReceiver::sigTransportError, this, &CameraFrameSource::sigTransportError);

    connect(receiver_, &CameraStreamReceiver::sigProtocolError, this, &CameraFrameSource::sigProtocolError);

    connect(receiver_, &CameraStreamReceiver::sigBufferOverflow, this, &CameraFrameSource::sigBufferOverflow);
}

bool CameraFrameSource::start(const QHostAddress& address,quint16 port,QString* errorMessage)
{
    return receiver_->start(address,port,errorMessage);
}

void CameraFrameSource::stop()
{
    receiver_->stop();
}

bool CameraFrameSource::isListening() const
{
    return receiver_->isListening();
}

quint16 CameraFrameSource::listeningPort() const
{
    return receiver_->listeningPort();
}

bool CameraFrameSource::hasClient() const
{
    return receiver_->hasClient();
}

qsizetype CameraFrameSource::bufferedBytes() const noexcept
{
    return receiver_->bufferedBytes();
}


qint64 CameraFrameSource::pendingWriteBytes() const
{
    return receiver_->pendingWriteBytes();
}


bool CameraFrameSource::sendInferenceResult(quint64 sequence, const InferenceResponseCodec::InferenceResultPayload& payload, QString* errorMessage)
{
    const QByteArray jsonByte = InferenceResponseCodec::encodeResult(payload,errorMessage);

    if(jsonByte.isEmpty())
    {
        return false;
    }

    return sendResponsePacket(FrameProtocol::MessageType::InferenceResult,sequence,jsonByte,errorMessage);
}


bool CameraFrameSource::sendInferenceError(quint64 sequence, const InferenceResponseCodec::InferenceErrorPayload& payload, QString* errorMessage)
{
    const QByteArray jsonByte = InferenceResponseCodec::encodeError(payload,errorMessage);

    if(jsonByte.isEmpty())
    {
        return false;
    }

    return sendResponsePacket(FrameProtocol::MessageType::InferenceError,sequence,jsonByte,errorMessage);
}


bool CameraFrameSource::sendResponsePacket(FrameProtocol::MessageType messageType, quint64 sequence, const QByteArray& payload, QString* errorMessage)
{
    FrameProtocol::FrameHeader header;
    header.messageType = messageType;
    header.pixelFormat = FrameProtocol::PixelFormat::Unknown;
    header.sequence = sequence;
    header.timestampMs = static_cast<quint64>(QDateTime::currentMSecsSinceEpoch());
    header.width = 0;
    header.height = 0;

    const QByteArray packet = FrameProtocol::encodePacket(header, payload);

    if(packet.isEmpty())
    {
        if(errorMessage)
        {
            *errorMessage = QStringLiteral("Failed to encode camera response packet");

        }
        return false;
    }

    return receiver_->sendBytes(packet,errorMessage);
}


void CameraFrameSource::onPacketReceived(const ReceivedFramePacket& packet)
{
    if(packet.header.messageType == FrameProtocol::MessageType::Heartbeat)
    {
        emit sigHeartbeatReceived(packet.header.sequence, packet.header.timestampMs);
        return;
    }

    if(packet.header.messageType != FrameProtocol::MessageType::ImageFrame)
    {
        emit sigProtocolError(QStringLiteral("Unsupported application message type"));
        return;
    }

    DecodedCameraFrame decodedFrame;
    QString errorMessage;

    if(!ReceivedFrameImageDecoder::decode(packet, &decodedFrame, &errorMessage))
    {
        emit sigImageDecodeError(packet.header.sequence, errorMessage);
        return;
    }

    emit sigFrameReady(decodedFrame);
}
