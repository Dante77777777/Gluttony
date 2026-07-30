#ifndef CAMERAFRAMESOURCE_H
#define CAMERAFRAMESOURCE_H

#include <QHostAddress>
#include <QObject>
#include <QString>
#include <QtGlobal>
#include <QMetaType>

#include "CameraStreamReceiver.h"
#include "ReceivedFrameImageDecoder.h"
#include "InferenceResponseCodec.h"


class CameraFrameSource final : public QObject
{
    Q_OBJECT

public:
    explicit CameraFrameSource(QObject* parent = nullptr);

    bool start(const QHostAddress& address,quint16 port,QString* errorMessage = nullptr);

    void stop();

    bool isListening() const;
    quint16 listeningPort() const;
    bool hasClient() const;
    qsizetype bufferedBytes() const noexcept;

    qint64 pendingWriteBytes() const;
    bool sendInferenceResult(quint64 sequence, const InferenceResponseCodec::InferenceResultPayload& payload, QString* errorMessage = nullptr);
    bool sendInferenceError(quint64 sequence, const InferenceResponseCodec::InferenceErrorPayload& payload, QString* errorMessage = nullptr);
    bool sendStopAcquisitionRequest(
        quint64 requestSequence,
        QString* errorMessage = nullptr);

signals:
    void sigServerStarted(quint16 port);
    void sigServerStopped();

    void sigClientConnected(const QString& peerAddress,quint16 peerPort);

    void sigClientDisconnected();
    void sigClientRejected(const QString& reason);

    void sigFrameReady(const DecodedCameraFrame& frame);

    void sigHeartbeatReceived(quint64 sequence,quint64 timestampMs);
    void sigStreamStarted(quint64 streamSequence, quint64 timestampMs);
    void sigStreamStopped(quint64 streamSequence, quint64 timestampMs);

    void sigTransportError(const QString& errorMessage);

    void sigProtocolError(const QString& errorMessage);

    void sigImageDecodeError(quint64 sequence,const QString& errorMessage);

    void sigBufferOverflow(qsizetype bufferedBytes);

private:
    bool validateControlPacket(
        const ReceivedFramePacket& packet,
        QString* errorMessage) const;
    bool sendResponsePacket(FrameProtocol::MessageType messageType, quint64 sequence, const QByteArray& payload, QString* errorMessage);
    void onPacketReceived(const ReceivedFramePacket& packet);

private:
    CameraStreamReceiver* receiver_ = nullptr;
};







#endif
