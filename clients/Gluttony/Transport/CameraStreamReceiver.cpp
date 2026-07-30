#include "CameraStreamReceiver.h"

CameraStreamReceiver::CameraStreamReceiver(QObject* parent)
    : QObject(parent)
    , server_(new TcpImageServer(this))
    , decoder_(new FrameStreamDecoder(this))
{
    connect(server_,&TcpImageServer::bytesReceived,decoder_,&FrameStreamDecoder::appendBytes);

    connect(server_,&TcpImageServer::sigServerStarted,this,&CameraStreamReceiver::sigServerStarted);

    connect(server_,&TcpImageServer::sigServerStopped,this,[this](){
        decoder_->reset();
        emit sigServerStopped();
    });

    connect(server_,&TcpImageServer::sigClientConnected,this,&CameraStreamReceiver::sigClientConnected);

    connect(server_,&TcpImageServer::sigClientDisconnected,this,[this](){
        decoder_->reset();
        emit sigClientDisconnected();
    });

    connect(server_,&TcpImageServer::sigClientRejected,this,&CameraStreamReceiver::sigClientRejected);

    connect(server_, &TcpImageServer::errorOccurred,this,&CameraStreamReceiver::sigTransportError);

    connect(decoder_,&FrameStreamDecoder::sigFrameDecoded,this,&CameraStreamReceiver::sigFrameReceived);

    connect(decoder_,&FrameStreamDecoder::sigProtocolError,this,&CameraStreamReceiver::sigProtocolError);

    connect(decoder_,&FrameStreamDecoder::sigBufferOverflow,this,&CameraStreamReceiver::sigBufferOverflow);
}


bool CameraStreamReceiver::start(const QHostAddress& address, quint16 port, QString* errorMessage)
{
    decoder_->reset();
    return server_->start(address,port,errorMessage);
}


void CameraStreamReceiver::stop()
{
    decoder_->reset();
    server_->stop();
}


bool CameraStreamReceiver::isListening() const
{
    return server_->isListening();
}


quint16 CameraStreamReceiver::listeningPort() const
{
    return server_->listeningPort();
}


bool CameraStreamReceiver::hasClient() const
{
    return server_->hasClient();
}


qsizetype CameraStreamReceiver::bufferedBytes() const noexcept
{
    return decoder_->bufferedBytes();
}


qint64 CameraStreamReceiver::pendingWriteBytes() const
{
    return server_->pendingWriteBytes();
}


bool CameraStreamReceiver::sendBytes(const QByteArray& bytes, QString* errorMessage)
{
    return server_->sendBytes(bytes,errorMessage);
}
