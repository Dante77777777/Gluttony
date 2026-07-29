#include "CameraFrameSource.h"

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
