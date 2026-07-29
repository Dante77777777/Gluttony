#include <QBuffer>
#include <QImage>
#include <QtTest>

#include "Transport/ReceivedFrameImageDecoder.h"

class ReceivedFrameImageDecoderTest : public QObject
{
    Q_OBJECT

private slots:
    void decodesValidJpegAndPreservesMetadata();
    void rejectsNullResult();
    void rejectsNonImageMessage();
    void rejectsUnsupportedPixelFormat();
    void rejectsEmptyPayload();
    void rejectsInvalidDimensions();
    void rejectsExcessiveDimensions();
    void rejectsCorruptedJpeg();
    void rejectsDimensionMismatch();
};

namespace
{

QByteArray makeJpeg(
    int width,
    int height)
{
    QImage sourceImage(
        width,
        height,
        QImage::Format_RGB888);
    sourceImage.fill(QColor(32, 96, 160));

    QByteArray payload;
    QBuffer buffer(&payload);
    if(!buffer.open(QIODevice::WriteOnly)
        || !sourceImage.save(&buffer, "JPEG", 90))
    {
        return {};
    }

    return payload;
}

ReceivedFramePacket makePacket(
    const QByteArray& payload,
    quint32 width = 64,
    quint32 height = 48)
{
    ReceivedFramePacket packet;
    packet.header.messageType =
        FrameProtocol::MessageType::ImageFrame;
    packet.header.pixelFormat =
        FrameProtocol::PixelFormat::Jpeg;
    packet.header.sequence = 123;
    packet.header.timestampMs = 1'785'211'200'123ULL;
    packet.header.width = width;
    packet.header.height = height;
    packet.header.payloadSize =
        static_cast<quint32>(payload.size());
    packet.payload = payload;
    return packet;
}

}

void ReceivedFrameImageDecoderTest::
    decodesValidJpegAndPreservesMetadata()
{
    const QByteArray payload = makeJpeg(64, 48);
    QVERIFY(!payload.isEmpty());

    const ReceivedFramePacket packet =
        makePacket(payload);
    DecodedCameraFrame result;
    QString errorMessage;

    QVERIFY(ReceivedFrameImageDecoder::decode(
        packet,
        &result,
        &errorMessage));

    QVERIFY(errorMessage.isEmpty());
    QVERIFY(!result.image.isNull());
    QCOMPARE(result.image.size(), QSize(64, 48));
    QCOMPARE(
        result.image.format(),
        QImage::Format_RGB888);
    QCOMPARE(result.header.sequence, 123ULL);
    QCOMPARE(
        result.header.timestampMs,
        1'785'211'200'123ULL);
    QCOMPARE(result.header.width, 64U);
    QCOMPARE(result.header.height, 48U);
    QCOMPARE(
        result.header.pixelFormat,
        FrameProtocol::PixelFormat::Jpeg);
}

void ReceivedFrameImageDecoderTest::rejectsNullResult()
{
    QString errorMessage;

    QVERIFY(!ReceivedFrameImageDecoder::decode(
        makePacket(makeJpeg(64, 48)),
        nullptr,
        &errorMessage));
    QVERIFY(!errorMessage.isEmpty());
}

void ReceivedFrameImageDecoderTest::
    rejectsNonImageMessage()
{
    ReceivedFramePacket packet =
        makePacket(makeJpeg(64, 48));
    packet.header.messageType =
        FrameProtocol::MessageType::Heartbeat;

    DecodedCameraFrame result;
    QString errorMessage;

    QVERIFY(!ReceivedFrameImageDecoder::decode(
        packet,
        &result,
        &errorMessage));
    QVERIFY(result.image.isNull());
    QVERIFY(!errorMessage.isEmpty());
}

void ReceivedFrameImageDecoderTest::
    rejectsUnsupportedPixelFormat()
{
    ReceivedFramePacket packet =
        makePacket(makeJpeg(64, 48));
    packet.header.pixelFormat =
        FrameProtocol::PixelFormat::Bgr8;

    DecodedCameraFrame result;
    QString errorMessage;

    QVERIFY(!ReceivedFrameImageDecoder::decode(
        packet,
        &result,
        &errorMessage));
    QVERIFY(result.image.isNull());
    QVERIFY(!errorMessage.isEmpty());
}

void ReceivedFrameImageDecoderTest::rejectsEmptyPayload()
{
    DecodedCameraFrame result;
    QString errorMessage;

    QVERIFY(!ReceivedFrameImageDecoder::decode(
        makePacket({}),
        &result,
        &errorMessage));
    QVERIFY(result.image.isNull());
    QVERIFY(!errorMessage.isEmpty());
}

void ReceivedFrameImageDecoderTest::
    rejectsInvalidDimensions()
{
    const QByteArray payload = makeJpeg(64, 48);
    DecodedCameraFrame result;
    QString errorMessage;

    QVERIFY(!ReceivedFrameImageDecoder::decode(
        makePacket(payload, 0, 48),
        &result,
        &errorMessage));
    QVERIFY(!errorMessage.isEmpty());

    QVERIFY(!ReceivedFrameImageDecoder::decode(
        makePacket(payload, 64, 0),
        &result,
        &errorMessage));
    QVERIFY(!errorMessage.isEmpty());
}

void ReceivedFrameImageDecoderTest::
    rejectsExcessiveDimensions()
{
    DecodedCameraFrame result;
    QString errorMessage;

    QVERIFY(!ReceivedFrameImageDecoder::decode(
        makePacket(
            QByteArray("not-read"),
            100'001,
            1'000),
        &result,
        &errorMessage));
    QVERIFY(!errorMessage.isEmpty());
}

void ReceivedFrameImageDecoderTest::
    rejectsCorruptedJpeg()
{
    DecodedCameraFrame result;
    QString errorMessage;

    QVERIFY(!ReceivedFrameImageDecoder::decode(
        makePacket(
            QByteArray("not-a-jpeg-payload")),
        &result,
        &errorMessage));
    QVERIFY(result.image.isNull());
    QVERIFY(!errorMessage.isEmpty());
}

void ReceivedFrameImageDecoderTest::
    rejectsDimensionMismatch()
{
    const QByteArray payload = makeJpeg(64, 48);
    QVERIFY(!payload.isEmpty());

    DecodedCameraFrame result;
    result.image = QImage(
        1,
        1,
        QImage::Format_RGB888);
    QString errorMessage;

    QVERIFY(!ReceivedFrameImageDecoder::decode(
        makePacket(payload, 63, 48),
        &result,
        &errorMessage));
    QVERIFY(result.image.isNull());
    QVERIFY(!errorMessage.isEmpty());
}

QTEST_GUILESS_MAIN(ReceivedFrameImageDecoderTest)

#include "ReceivedFrameImageDecoderTest.moc"
