#include <QHostAddress>
#include <QSignalSpy>
#include <QTcpSocket>
#include <QtTest>

#include "Transport/TcpImageServer.h"

class TcpImageServerTest : public QObject
{
    Q_OBJECT

private slots:
    void startsAndStopsListening();
    void acceptsClientAndReceivesBytes();
    void clearsClientAfterRemoteDisconnect();
    void rejectsSecondClient();
};

namespace
{

QByteArray receivedBytes(
    const QSignalSpy& bytesSpy
)
{
    QByteArray result;

    for(const QList<QVariant>& arguments : bytesSpy)
    {
        result.append(
            arguments.at(0).toByteArray()
        );
    }

    return result;
}

}

void TcpImageServerTest::startsAndStopsListening()
{
    TcpImageServer server;
    QSignalSpy startedSpy(
        &server,
        &TcpImageServer::sigServerStarted
    );
    QSignalSpy stoppedSpy(
        &server,
        &TcpImageServer::sigServerStopped
    );
    QString errorMessage;

    QVERIFY(
        server.start(
            QHostAddress::LocalHost,
            0,
            &errorMessage
        )
    );

    QVERIFY(errorMessage.isEmpty());
    QVERIFY(server.isListening());
    QVERIFY(server.listeningPort() != 0);
    QVERIFY(!server.hasClient());
    QCOMPARE(startedSpy.count(), 1);
    QCOMPARE(
        startedSpy.first().at(0).toUInt(),
        static_cast<uint>(server.listeningPort())
    );

    server.stop();

    QVERIFY(!server.isListening());
    QVERIFY(!server.hasClient());
    QCOMPARE(stoppedSpy.count(), 1);
}

void TcpImageServerTest::
    acceptsClientAndReceivesBytes()
{
    TcpImageServer server;
    QVERIFY(
        server.start(
            QHostAddress::LocalHost,
            0
        )
    );

    QSignalSpy connectedSpy(
        &server,
        &TcpImageServer::sigClientConnected
    );
    QSignalSpy bytesSpy(
        &server,
        &TcpImageServer::bytesReceived
    );
    QSignalSpy errorSpy(
        &server,
        &TcpImageServer::errorOccurred
    );

    QTcpSocket client;
    client.connectToHost(
        QHostAddress::LocalHost,
        server.listeningPort()
    );

    QTRY_COMPARE_WITH_TIMEOUT(
        connectedSpy.count(),
        1,
        2000
    );
    QTRY_VERIFY_WITH_TIMEOUT(
        server.hasClient(),
        2000
    );

    const QByteArray expectedBytes(
        "lust-frame-protocol-bytes"
    );

    QCOMPARE(
        client.write(expectedBytes),
        static_cast<qint64>(
            expectedBytes.size()
        )
    );

    QTRY_VERIFY_WITH_TIMEOUT(
        receivedBytes(bytesSpy).size()
            >= expectedBytes.size(),
        2000
    );

    QCOMPARE(
        receivedBytes(bytesSpy),
        expectedBytes
    );
    QCOMPARE(errorSpy.count(), 0);
}

void TcpImageServerTest::
    clearsClientAfterRemoteDisconnect()
{
    TcpImageServer server;
    QVERIFY(
        server.start(
            QHostAddress::LocalHost,
            0
        )
    );

    QSignalSpy connectedSpy(
        &server,
        &TcpImageServer::sigClientConnected
    );
    QSignalSpy disconnectedSpy(
        &server,
        &TcpImageServer::sigClientDisconnected
    );

    QTcpSocket client;
    client.connectToHost(
        QHostAddress::LocalHost,
        server.listeningPort()
    );

    QTRY_COMPARE_WITH_TIMEOUT(
        connectedSpy.count(),
        1,
        2000
    );

    client.disconnectFromHost();

    QTRY_COMPARE_WITH_TIMEOUT(
        disconnectedSpy.count(),
        1,
        2000
    );
    QTRY_VERIFY_WITH_TIMEOUT(
        !server.hasClient(),
        2000
    );
    QVERIFY(server.isListening());
}

void TcpImageServerTest::rejectsSecondClient()
{
    TcpImageServer server;
    QVERIFY(
        server.start(
            QHostAddress::LocalHost,
            0
        )
    );

    QSignalSpy connectedSpy(
        &server,
        &TcpImageServer::sigClientConnected
    );
    QSignalSpy rejectedSpy(
        &server,
        &TcpImageServer::sigClientRejected
    );

    QTcpSocket firstClient;
    firstClient.connectToHost(
        QHostAddress::LocalHost,
        server.listeningPort()
    );

    QTRY_COMPARE_WITH_TIMEOUT(
        connectedSpy.count(),
        1,
        2000
    );
    QTRY_VERIFY_WITH_TIMEOUT(
        server.hasClient(),
        2000
    );

    QTcpSocket secondClient;
    secondClient.connectToHost(
        QHostAddress::LocalHost,
        server.listeningPort()
    );

    QTRY_COMPARE_WITH_TIMEOUT(
        rejectedSpy.count(),
        1,
        2000
    );
    QCOMPARE(connectedSpy.count(), 1);
    QVERIFY(server.hasClient());

    QTRY_COMPARE_WITH_TIMEOUT(
        secondClient.state(),
        QAbstractSocket::UnconnectedState,
        2000
    );
}

QTEST_GUILESS_MAIN(TcpImageServerTest)

#include "TcpImageServerTest.moc"
