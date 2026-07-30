#include <QHostAddress>
#include <QSignalSpy>
#include <QTcpSocket>
#include <QtTest>

#include "Transport/CameraStreamReceiver.h"
#include "Transport/FrameProtocol.h"

class CameraStreamReceiverTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void relaysServerLifecycle();
    void decodesFragmentedAndStickyPackets();
    void clearsPartialPacketAfterDisconnect();
    void relaysBytesToConnectedClient();
    void propagatesWriteFailureWithoutClient();
};

namespace
{

FrameProtocol::FrameHeader makeHeader(
    quint64 sequence,
    quint32 width,
    quint32 height)
{
    FrameProtocol::FrameHeader header;
    header.messageType = FrameProtocol::MessageType::ImageFrame;
    header.pixelFormat = FrameProtocol::PixelFormat::Jpeg;
    header.sequence = sequence;
    header.timestampMs = 1'700'000'000'000ULL + sequence;
    header.width = width;
    header.height = height;
    return header;
}

void connectClient(
    CameraStreamReceiver& receiver,
    QTcpSocket& client)
{
    QSignalSpy connectedSpy(
        &receiver,
        &CameraStreamReceiver::sigClientConnected);

    client.connectToHost(
        QHostAddress::LocalHost,
        receiver.listeningPort());

    QTRY_COMPARE_WITH_TIMEOUT(
        connectedSpy.count(),
        1,
        2000);
    QTRY_VERIFY_WITH_TIMEOUT(
        receiver.hasClient(),
        2000);
}

}

void CameraStreamReceiverTest::initTestCase()
{
    qRegisterMetaType<ReceivedFramePacket>(
        "ReceivedFramePacket");
}

void CameraStreamReceiverTest::relaysServerLifecycle()
{
    CameraStreamReceiver receiver;
    QSignalSpy startedSpy(
        &receiver,
        &CameraStreamReceiver::sigServerStarted);
    QSignalSpy stoppedSpy(
        &receiver,
        &CameraStreamReceiver::sigServerStopped);
    QString errorMessage;

    QVERIFY(receiver.start(
        QHostAddress::LocalHost,
        0,
        &errorMessage));

    QVERIFY(errorMessage.isEmpty());
    QVERIFY(receiver.isListening());
    QVERIFY(receiver.listeningPort() != 0);
    QCOMPARE(startedSpy.count(), 1);
    QCOMPARE(stoppedSpy.count(), 0);

    receiver.stop();

    QVERIFY(!receiver.isListening());
    QCOMPARE(stoppedSpy.count(), 1);
}

void CameraStreamReceiverTest::
    decodesFragmentedAndStickyPackets()
{
    CameraStreamReceiver receiver;
    QVERIFY(receiver.start(
        QHostAddress::LocalHost,
        0));

    QTcpSocket client;
    connectClient(receiver, client);

    QSignalSpy frameSpy(
        &receiver,
        &CameraStreamReceiver::sigFrameReceived);
    QSignalSpy protocolErrorSpy(
        &receiver,
        &CameraStreamReceiver::sigProtocolError);

    const QByteArray firstPayload("first-jpeg-payload");
    const QByteArray secondPayload("second-jpeg-payload");
    const QByteArray firstPacket =
        FrameProtocol::encodePacket(
            makeHeader(41, 640, 480),
            firstPayload);
    const QByteArray secondPacket =
        FrameProtocol::encodePacket(
            makeHeader(42, 1280, 720),
            secondPayload);

    const qsizetype splitPosition = 13;
    QCOMPARE(
        client.write(firstPacket.left(splitPosition)),
        static_cast<qint64>(splitPosition));
    QVERIFY(client.waitForBytesWritten(1000));

    QTest::qWait(20);
    QCOMPARE(frameSpy.count(), 0);

    const QByteArray remainingAndNext =
        firstPacket.mid(splitPosition) + secondPacket;
    QCOMPARE(
        client.write(remainingAndNext),
        static_cast<qint64>(remainingAndNext.size()));

    QTRY_COMPARE_WITH_TIMEOUT(
        frameSpy.count(),
        2,
        2000);

    const ReceivedFramePacket firstFrame =
        qvariant_cast<ReceivedFramePacket>(
            frameSpy.at(0).at(0));
    const ReceivedFramePacket secondFrame =
        qvariant_cast<ReceivedFramePacket>(
            frameSpy.at(1).at(0));

    QCOMPARE(firstFrame.header.sequence, 41ULL);
    QCOMPARE(firstFrame.header.width, 640U);
    QCOMPARE(firstFrame.header.height, 480U);
    QCOMPARE(firstFrame.payload, firstPayload);

    QCOMPARE(secondFrame.header.sequence, 42ULL);
    QCOMPARE(secondFrame.header.width, 1280U);
    QCOMPARE(secondFrame.header.height, 720U);
    QCOMPARE(secondFrame.payload, secondPayload);

    QCOMPARE(protocolErrorSpy.count(), 0);
    QCOMPARE(receiver.bufferedBytes(), 0);
}

void CameraStreamReceiverTest::
    clearsPartialPacketAfterDisconnect()
{
    CameraStreamReceiver receiver;
    QVERIFY(receiver.start(
        QHostAddress::LocalHost,
        0));

    const QByteArray packet =
        FrameProtocol::encodePacket(
            makeHeader(99, 320, 240),
            QByteArray("complete-payload"));
    const qsizetype splitPosition =
        FrameProtocol::HEADER_SIZE + 3;

    QSignalSpy disconnectedSpy(
        &receiver,
        &CameraStreamReceiver::sigClientDisconnected);
    QSignalSpy frameSpy(
        &receiver,
        &CameraStreamReceiver::sigFrameReceived);

    {
        QTcpSocket firstClient;
        connectClient(receiver, firstClient);

        QCOMPARE(
            firstClient.write(
                packet.left(splitPosition)),
            static_cast<qint64>(splitPosition));
        QVERIFY(firstClient.waitForBytesWritten(1000));

        QTRY_COMPARE_WITH_TIMEOUT(
            receiver.bufferedBytes(),
            splitPosition,
            2000);

        firstClient.disconnectFromHost();
        QTRY_COMPARE_WITH_TIMEOUT(
            disconnectedSpy.count(),
            1,
            2000);
    }

    QCOMPARE(receiver.bufferedBytes(), 0);

    QTcpSocket secondClient;
    connectClient(receiver, secondClient);
    QCOMPARE(
        secondClient.write(packet),
        static_cast<qint64>(packet.size()));

    QTRY_COMPARE_WITH_TIMEOUT(
        frameSpy.count(),
        1,
        2000);

    const ReceivedFramePacket frame =
        qvariant_cast<ReceivedFramePacket>(
            frameSpy.first().at(0));
    QCOMPARE(frame.header.sequence, 99ULL);
    QCOMPARE(
        frame.payload,
        QByteArray("complete-payload"));
}


void CameraStreamReceiverTest::
    relaysBytesToConnectedClient()
{
    CameraStreamReceiver receiver;
    QVERIFY(receiver.start(
        QHostAddress::LocalHost,
        0));

    QTcpSocket client;
    connectClient(receiver, client);

    const QByteArray expectedBytes(
        "receiver-response-bytes");
    QString errorMessage;

    QVERIFY2(
        receiver.sendBytes(
            expectedBytes,
            &errorMessage),
        qPrintable(errorMessage));
    QVERIFY(errorMessage.isEmpty());

    QTRY_VERIFY_WITH_TIMEOUT(
        client.bytesAvailable()
            >= expectedBytes.size(),
        2000);
    QCOMPARE(client.readAll(), expectedBytes);
}


void CameraStreamReceiverTest::
    propagatesWriteFailureWithoutClient()
{
    CameraStreamReceiver receiver;
    QVERIFY(receiver.start(
        QHostAddress::LocalHost,
        0));

    QString errorMessage;
    QVERIFY(
        !receiver.sendBytes(
            QByteArray("response"),
            &errorMessage));
    QCOMPARE(
        errorMessage,
        QStringLiteral(
            "No connected camera client"));
    QCOMPARE(receiver.pendingWriteBytes(), 0);
}


QTEST_GUILESS_MAIN(CameraStreamReceiverTest)

#include "CameraStreamReceiverTest.moc"
