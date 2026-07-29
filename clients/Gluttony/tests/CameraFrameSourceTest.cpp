#include <QBuffer>
#include <QHostAddress>
#include <QImage>
#include <QSignalSpy>
#include <QTcpSocket>
#include <QtTest>

#include "Transport/CameraFrameSource.h"
#include "Transport/FrameProtocol.h"

class CameraFrameSourceTest : public QObject
{
    Q_OBJECT

private slots:
    void receivesFragmentedJpegOverTcp();
    void routesHeartbeatWithoutImageDecode();
    void reportsCorruptedJpeg();
};

namespace
{

QByteArray makeJpeg(
    int width,
    int height)
{
    QImage image(
        width,
        height,
        QImage::Format_RGB888);
    image.fill(QColor(48, 112, 176));

    QByteArray payload;
    QBuffer buffer(&payload);
    if(!buffer.open(QIODevice::WriteOnly)
        || !image.save(&buffer, "JPEG", 90))
    {
        return {};
    }

    return payload;
}

QByteArray makeImagePacket(
    quint64 sequence,
    const QByteArray& payload,
    quint32 width,
    quint32 height)
{
    FrameProtocol::FrameHeader header;
    header.messageType =
        FrameProtocol::MessageType::ImageFrame;
    header.pixelFormat =
        FrameProtocol::PixelFormat::Jpeg;
    header.sequence = sequence;
    header.timestampMs =
        1'785'211'200'000ULL + sequence;
    header.width = width;
    header.height = height;

    return FrameProtocol::encodePacket(
        header,
        payload);
}

void connectClient(
    CameraFrameSource& source,
    QTcpSocket& client)
{
    QSignalSpy connectedSpy(
        &source,
        &CameraFrameSource::sigClientConnected);

    client.connectToHost(
        QHostAddress::LocalHost,
        source.listeningPort());

    QTRY_COMPARE_WITH_TIMEOUT(
        connectedSpy.count(),
        1,
        2000);
    QTRY_VERIFY_WITH_TIMEOUT(
        source.hasClient(),
        2000);
}

}

void CameraFrameSourceTest::
    receivesFragmentedJpegOverTcp()
{
    CameraFrameSource source;
    QVERIFY(source.start(
        QHostAddress::LocalHost,
        0));

    QTcpSocket client;
    connectClient(source, client);

    QSignalSpy frameSpy(
        &source,
        &CameraFrameSource::sigFrameReady);
    QSignalSpy decodeErrorSpy(
        &source,
        &CameraFrameSource::sigImageDecodeError);

    const QByteArray payload = makeJpeg(80, 60);
    QVERIFY(!payload.isEmpty());

    const QByteArray packet = makeImagePacket(
        501,
        payload,
        80,
        60);
    QVERIFY(!packet.isEmpty());

    const qsizetype splitPosition = 17;
    QCOMPARE(
        client.write(packet.left(splitPosition)),
        static_cast<qint64>(splitPosition));
    QVERIFY(client.waitForBytesWritten(1000));

    QTest::qWait(20);
    QCOMPARE(frameSpy.count(), 0);

    QCOMPARE(
        client.write(packet.mid(splitPosition)),
        static_cast<qint64>(
            packet.size() - splitPosition));

    QTRY_COMPARE_WITH_TIMEOUT(
        frameSpy.count(),
        1,
        2000);

    const DecodedCameraFrame frame =
        qvariant_cast<DecodedCameraFrame>(
            frameSpy.first().at(0));

    QCOMPARE(frame.header.sequence, 501ULL);
    QCOMPARE(frame.header.width, 80U);
    QCOMPARE(frame.header.height, 60U);
    QCOMPARE(frame.image.size(), QSize(80, 60));
    QCOMPARE(
        frame.image.format(),
        QImage::Format_RGB888);
    QCOMPARE(decodeErrorSpy.count(), 0);
}

void CameraFrameSourceTest::
    routesHeartbeatWithoutImageDecode()
{
    CameraFrameSource source;
    QVERIFY(source.start(
        QHostAddress::LocalHost,
        0));

    QTcpSocket client;
    connectClient(source, client);

    QSignalSpy heartbeatSpy(
        &source,
        &CameraFrameSource::sigHeartbeatReceived);
    QSignalSpy frameSpy(
        &source,
        &CameraFrameSource::sigFrameReady);
    QSignalSpy decodeErrorSpy(
        &source,
        &CameraFrameSource::sigImageDecodeError);

    FrameProtocol::FrameHeader header;
    header.messageType =
        FrameProtocol::MessageType::Heartbeat;
    header.pixelFormat =
        FrameProtocol::PixelFormat::Unknown;
    header.sequence = 700;
    header.timestampMs = 1'785'211'200'700ULL;

    const QByteArray packet =
        FrameProtocol::encodePacket(header, {});
    QCOMPARE(
        client.write(packet),
        static_cast<qint64>(packet.size()));

    QTRY_COMPARE_WITH_TIMEOUT(
        heartbeatSpy.count(),
        1,
        2000);

    QCOMPARE(
        heartbeatSpy.first().at(0).toULongLong(),
        700ULL);
    QCOMPARE(
        heartbeatSpy.first().at(1).toULongLong(),
        1'785'211'200'700ULL);
    QCOMPARE(frameSpy.count(), 0);
    QCOMPARE(decodeErrorSpy.count(), 0);
}

void CameraFrameSourceTest::reportsCorruptedJpeg()
{
    CameraFrameSource source;
    QVERIFY(source.start(
        QHostAddress::LocalHost,
        0));

    QTcpSocket client;
    connectClient(source, client);

    QSignalSpy frameSpy(
        &source,
        &CameraFrameSource::sigFrameReady);
    QSignalSpy decodeErrorSpy(
        &source,
        &CameraFrameSource::sigImageDecodeError);

    const QByteArray packet = makeImagePacket(
        900,
        QByteArray("corrupted-jpeg"),
        64,
        48);
    QCOMPARE(
        client.write(packet),
        static_cast<qint64>(packet.size()));

    QTRY_COMPARE_WITH_TIMEOUT(
        decodeErrorSpy.count(),
        1,
        2000);

    QCOMPARE(
        decodeErrorSpy.first().at(0).toULongLong(),
        900ULL);
    QVERIFY(
        !decodeErrorSpy.first().at(1).toString()
            .isEmpty());
    QCOMPARE(frameSpy.count(), 0);
}

QTEST_GUILESS_MAIN(CameraFrameSourceTest)

#include "CameraFrameSourceTest.moc"
