#include "TcpImageServer.h"

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

    connect(socket,&QTcpSocket::errorOccurred,this,[this,socket](QAbstractSocket::SocketError){
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
