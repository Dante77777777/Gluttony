#ifndef TCPIMAGESERVER_H
#define TCPIMAGESERVER_H

#include <QHostAddress>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QAbstractSocket>
#include <QTcpServer>
#include <QTcpSocket>
#include <QByteArray>
#include <QtGlobal>

class QTcpServer;
class QTcpSocket;

class TcpImageServer final : public QObject
{
    Q_OBJECT
public:
    static constexpr qint64 MAX_PENDING_WRITE_BYTES = 1024 * 1024;
    explicit TcpImageServer(QObject* parent = nullptr);

    ~TcpImageServer() override;

    bool start(const QHostAddress& address, quint16 port, QString* errorMessage = nullptr);

    void stop();

    bool isListening() const;

    quint16 listeningPort() const;

    bool hasClient() const;

    qint64 pendingWriteBytes() const;

    bool sendBytes(const QByteArray& bytes, QString* errorMessage = nullptr);
signals:
    void sigServerStarted(quint16 port);

    void sigServerStopped();

    void sigClientConnected(const QString& peerAddress, quint16 peerPort);

    void sigClientDisconnected();

    void sigClientRejected(const QString& reason);

    /*
     * 这里只表示收到一段 TCP 字节。
     * 一段字节不一定等于一个完整图像包。
     */
    void bytesReceived(const QByteArray& bytes);

    void errorOccurred(const QString& errorMessage);
private:
    void onNewConnection();
    void attachClient(QTcpSocket* socket);

private:
    QTcpServer* server_ = nullptr;
    QPointer<QTcpSocket> client_;
};




#endif
