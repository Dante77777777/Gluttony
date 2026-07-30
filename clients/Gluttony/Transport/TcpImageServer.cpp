#include "TcpImageServer.h"

namespace
{

void setSendError(
    QString* errorMessage,
    const QString& message)
{
    if(errorMessage != nullptr)
    {
        *errorMessage = message;
    }
}

}


TcpImageServer::TcpImageServer(QObject* parent)
    : QObject(parent)
    , server_(new QTcpServer(this))
{
    connect(server_,&QTcpServer::newConnection,this,&TcpImageServer::onNewConnection);

    connect(server_,&QTcpServer::acceptError,this,[this](){
        emit errorOccurred(server_->errorString());
    });
}


TcpImageServer::~TcpImageServer()
{
    stop();
}


bool TcpImageServer::start(const QHostAddress& address, quint16 port, QString* errorMessage)
{
    if(errorMessage != nullptr)
    {
        errorMessage->clear();
    }

    if(server_->isListening() || hasClient())
    {
        stop();
    }

    if(!server_->listen(address,port))
    {
        const QString message = server_->errorString();
        if(errorMessage != nullptr)
        {
            *errorMessage = message;
        }
        emit errorOccurred(message);
        return false;
    }

    emit sigServerStarted(server_->serverPort());
    return true;
}


void TcpImageServer::stop()
{
    if(!client_.isNull())
    {
        QTcpSocket* socket = client_.data();
        client_.clear();

        QObject::disconnect(socket,nullptr,this,nullptr);

        socket->abort();
        socket->deleteLater();

        emit sigClientDisconnected();
    }

    if(server_->isListening())
    {
        server_->close();
        emit sigServerStopped();
    }
}


bool TcpImageServer::isListening() const
{
    return server_->isListening();
}


quint16 TcpImageServer::listeningPort() const
{
    return server_->serverPort();
}


bool TcpImageServer::hasClient() const
{
    return !client_.isNull() && client_->state() != QAbstractSocket::UnconnectedState;
}


void TcpImageServer::onNewConnection()
{
    while(server_->hasPendingConnections())
    {
        QTcpSocket* socket = server_->nextPendingConnection();
        if(socket == nullptr)
        {
            continue;
        }

        if(hasClient())
        {
            emit sigClientRejected(QStringLiteral("Only one camera client is allowed"));
            socket->abort();
            socket->deleteLater();
            continue;
        }

        attachClient(socket);
    }
}


void TcpImageServer::attachClient(QTcpSocket* socket)
{
    client_ = socket;

    socket->setSocketOption(QAbstractSocket::LowDelayOption,1);

    connect(socket,&QTcpSocket::readyRead,this,[this,socket](){
        if(client_.data() != socket)
        {
            return ;
        }
        const QByteArray bytes = socket->readAll();
        if(!bytes.isEmpty())
        {
            emit bytesReceived(bytes);
        }
    });

    connect(socket,&QTcpSocket::errorOccurred,this,[this,socket](QAbstractSocket::SocketError socketError){
        if(socketError == QAbstractSocket::RemoteHostClosedError)
        {
            return ;
        }

        if(client_.data() == socket)
        {
            emit errorOccurred(socket->errorString());
        }
    });

    connect(socket,&QTcpSocket::disconnected,this,[this,socket](){
        const bool wasActiveClient = client_.data() == socket;
        if(wasActiveClient)
        {
            client_.clear();
            emit sigClientDisconnected();
        }
        socket->deleteLater();
    });

    emit sigClientConnected(socket->peerAddress().toString(),socket->peerPort());
}


qint64 TcpImageServer::pendingWriteBytes() const
{
    if(client_.isNull())
    {
        return 0;
    }

    return client_->bytesToWrite();
}


bool TcpImageServer::sendBytes(const QByteArray& bytes, QString* errorMessage)
{
    if(errorMessage)
    {
        errorMessage->clear();
    }

    if(bytes.isEmpty())
    {
        setSendError(errorMessage,QStringLiteral("Cannot send an empty packet"));
        return false;
    }

    if(client_.isNull() || client_->state() != QAbstractSocket::ConnectedState)
    {
        setSendError(errorMessage, QStringLiteral("No connected camera client"));
        return false;
    }

    QTcpSocket* socket = client_.data();

    const qint64 byteCount = static_cast<qint64>(bytes.size());

    const qint64 pendingBytes = socket->bytesToWrite();

    if(byteCount > MAX_PENDING_WRITE_BYTES || pendingBytes> MAX_PENDING_WRITE_BYTES - byteCount)
    {
        setSendError(errorMessage, QStringLiteral("Camera client write buffer limit exceeded"));
        return false;
    }

    const qint64 writtenBytes = socket->write(bytes);

    if(writtenBytes != byteCount)
    {
        QString message = socket->errorString();

        if(message.isEmpty())
        {
            message = QStringLiteral("Failed to queue complete camera response");
        }

        setSendError(errorMessage, message);
        return false;
    }

    return true;
}
