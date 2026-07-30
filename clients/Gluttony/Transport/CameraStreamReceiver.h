#ifndef CAMERASTREAMRECEIVER_H
#define CAMERASTREAMRECEIVER_H

#include <QHostAddress>
#include <QObject>
#include <QString>
#include <QByteArray>

#include "FrameStreamDecoder.h"
#include "TcpImageServer.h"


class CameraStreamReceiver final : public QObject
{
    Q_OBJECT
public:
    explicit CameraStreamReceiver(QObject* parent = nullptr);

    bool start(const QHostAddress& address, quint16 port, QString* errorMessage = nullptr);

    void stop();

    bool isListening() const;
    quint16 listeningPort() const;
    bool hasClient() const;
    qsizetype bufferedBytes() const noexcept;
    qint64 pendingWriteBytes() const;
    bool sendBytes(const QByteArray& bytes, QString* errorMessage = nullptr);

signals:
    void sigServerStarted(quint16 port);
    void sigServerStopped();

    void sigClientConnected(const QString& peerAddress,quint16 peerPort);

    void sigClientDisconnected();
    void sigClientRejected(const QString& reason);

    void sigFrameReceived(const ReceivedFramePacket& frame);

    void sigTransportError(const QString& errorMessage);
    void sigProtocolError(const QString& errorMessage);
    void sigBufferOverflow(qsizetype bufferedBytes);
private:
    TcpImageServer* server_ = nullptr;
    FrameStreamDecoder* decoder_ = nullptr;
};






#endif
