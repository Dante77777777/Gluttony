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
    void routesStreamLifecyclePackets();
    void reportsCorruptedJpeg();
    void sendsInferenceResultPacket();
    void sendsInferenceErrorPacket();
    void sendsStopAcquisitionRequestPacket();
    void rejectsInvalidResponsePayload();
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

bool peekCompletePacket(
    QTcpSocket& client,
    FrameProtocol::FrameHeader* header,
    QByteArray* payload)
{
    if(header == nullptr || payload == nullptr)
    {
        return false;
    }

    QByteArray receiveBuffer =
        client.peek(client.bytesAvailable());
    QString errorMessage;

    return FrameProtocol::tryDecodePacket(
        receiveBuffer,
        *header,
        *payload,
        &errorMessage)
        == FrameProtocol::DecodeResult::Complete;
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

void CameraFrameSourceTest::
    routesStreamLifecyclePackets()
{
    CameraFrameSource source;
    QVERIFY(source.start(
        QHostAddress::LocalHost,
        0));

    QTcpSocket client;
    connectClient(source, client);

    QSignalSpy startedSpy(
        &source,
        &CameraFrameSource::sigStreamStarted);
    QSignalSpy stoppedSpy(
        &source,
        &CameraFrameSource::sigStreamStopped);
    QSignalSpy protocolErrorSpy(
        &source,
        &CameraFrameSource::sigProtocolError);

    FrameProtocol::FrameHeader header;
    header.messageType =
        FrameProtocol::MessageType::StreamStart;
    header.pixelFormat =
        FrameProtocol::PixelFormat::Unknown;
    header.sequence = 801;
    header.timestampMs = 1'785'211'200'801ULL;

    QByteArray packet =
        FrameProtocol::encodePacket(header, {});
    QCOMPARE(
        client.write(packet),
        static_cast<qint64>(packet.size()));
    QTRY_COMPARE_WITH_TIMEOUT(
        startedSpy.count(),
        1,
        2000);

    header.messageType =
        FrameProtocol::MessageType::StreamStop;
    packet = FrameProtocol::encodePacket(header, {});
    QCOMPARE(
        client.write(packet),
        static_cast<qint64>(packet.size()));
    QTRY_COMPARE_WITH_TIMEOUT(
        stoppedSpy.count(),
        1,
        2000);

    QCOMPARE(
        startedSpy.first().at(0).toULongLong(),
        801ULL);
    QCOMPARE(
        stoppedSpy.first().at(0).toULongLong(),
        801ULL);
    QCOMPARE(protocolErrorSpy.count(), 0);
    QVERIFY(source.hasClient());
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


void CameraFrameSourceTest::sendsInferenceResultPacket()
{
    CameraFrameSource source;
    QVERIFY(source.start(
        QHostAddress::LocalHost,
        0));

    QTcpSocket client;
    connectClient(source, client);

    InferenceResponseCodec::InferenceResultPayload
        expected;
    expected.detectionLevel = QStringLiteral("NG");
    expected.score = 0.873;
    expected.inferenceMs = 18;
    expected.defectArea = 126;

    QString errorMessage;
    QVERIFY2(
        source.sendInferenceResult(
            1001,
            expected,
            &errorMessage),
        qPrintable(errorMessage));

    FrameProtocol::FrameHeader header;
    QByteArray jsonBytes;
    QTRY_VERIFY_WITH_TIMEOUT(
        peekCompletePacket(
            client,
            &header,
            &jsonBytes),
        2000);

    QCOMPARE(
        header.messageType,
        FrameProtocol::MessageType::InferenceResult);
    QCOMPARE(
        header.pixelFormat,
        FrameProtocol::PixelFormat::Unknown);
    QCOMPARE(header.sequence, 1001ULL);
    QVERIFY(header.timestampMs > 0);
    QCOMPARE(header.width, 0U);
    QCOMPARE(header.height, 0U);

    InferenceResponseCodec::InferenceResultPayload
        actual;
    QVERIFY2(
        InferenceResponseCodec::decodeResult(
            jsonBytes,
            &actual,
            &errorMessage),
        qPrintable(errorMessage));
    QCOMPARE(
        actual.detectionLevel,
        expected.detectionLevel);
    QCOMPARE(actual.score, expected.score);
    QCOMPARE(actual.inferenceMs, expected.inferenceMs);
    QCOMPARE(actual.defectArea, expected.defectArea);
}


void CameraFrameSourceTest::sendsInferenceErrorPacket()
{
    CameraFrameSource source;
    QVERIFY(source.start(
        QHostAddress::LocalHost,
        0));

    QTcpSocket client;
    connectClient(source, client);

    InferenceResponseCodec::InferenceErrorPayload
        expected;
    expected.errorCode =
        QStringLiteral("FRAME_DROPPED");
    expected.message =
        QStringLiteral("Replaced by a newer frame");

    QString errorMessage;
    QVERIFY2(
        source.sendInferenceError(
            1002,
            expected,
            &errorMessage),
        qPrintable(errorMessage));

    FrameProtocol::FrameHeader header;
    QByteArray jsonBytes;
    QTRY_VERIFY_WITH_TIMEOUT(
        peekCompletePacket(
            client,
            &header,
            &jsonBytes),
        2000);

    QCOMPARE(
        header.messageType,
        FrameProtocol::MessageType::InferenceError);
    QCOMPARE(header.sequence, 1002ULL);

    InferenceResponseCodec::InferenceErrorPayload actual;
    QVERIFY2(
        InferenceResponseCodec::decodeError(
            jsonBytes,
            &actual,
            &errorMessage),
        qPrintable(errorMessage));
    QCOMPARE(actual.errorCode, expected.errorCode);
    QCOMPARE(actual.message, expected.message);
}

void CameraFrameSourceTest::
    sendsStopAcquisitionRequestPacket()
{
    CameraFrameSource source;
    QVERIFY(source.start(
        QHostAddress::LocalHost,
        0));

    QTcpSocket client;
    connectClient(source, client);

    QString errorMessage;
    QVERIFY2(
        source.sendStopAcquisitionRequest(
            1100,
            &errorMessage),
        qPrintable(errorMessage));

    FrameProtocol::FrameHeader header;
    QByteArray payload;
    QTRY_VERIFY_WITH_TIMEOUT(
        peekCompletePacket(client, &header, &payload),
        2000);

    QCOMPARE(
        header.messageType,
        FrameProtocol::MessageType::
            StopAcquisitionRequest);
    QCOMPARE(
        header.pixelFormat,
        FrameProtocol::PixelFormat::Unknown);
    QCOMPARE(header.sequence, 1100ULL);
    QCOMPARE(header.width, 0U);
    QCOMPARE(header.height, 0U);
    QVERIFY(payload.isEmpty());
}


void CameraFrameSourceTest::
    rejectsInvalidResponsePayload()
{
    CameraFrameSource source;
    QVERIFY(source.start(
        QHostAddress::LocalHost,
        0));

    QTcpSocket client;
    connectClient(source, client);

    InferenceResponseCodec::InferenceResultPayload
        invalidPayload;
    invalidPayload.detectionLevel =
        QStringLiteral("UNKNOWN");

    QString errorMessage;
    QVERIFY(
        !source.sendInferenceResult(
            1003,
            invalidPayload,
            &errorMessage));
    QVERIFY(!errorMessage.isEmpty());
    QCOMPARE(client.bytesAvailable(), 0);
    QCOMPARE(source.pendingWriteBytes(), 0);
}


QTEST_GUILESS_MAIN(CameraFrameSourceTest)

#include "CameraFrameSourceTest.moc"
