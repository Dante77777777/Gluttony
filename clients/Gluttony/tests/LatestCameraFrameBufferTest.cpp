#include <QColor>
#include <QImage>
#include <QtTest>

#include "Workers/LatestCameraFrameBuffer.h"

class LatestCameraFrameBufferTest : public QObject
{
    Q_OBJECT

private slots:
    void startsEmpty();
    void storesAndTakesSingleFrame();
    void replacesPendingFrameWithLatest();
    void resetClearsFrameAndStatistics();
};

namespace
{

DecodedCameraFrame makeFrame(
    quint64 sequence,
    const QColor& color)
{
    DecodedCameraFrame frame;
    frame.header.messageType =
        FrameProtocol::MessageType::ImageFrame;
    frame.header.pixelFormat =
        FrameProtocol::PixelFormat::Jpeg;
    frame.header.sequence = sequence;
    frame.header.timestampMs =
        1'785'211'200'000ULL + sequence;
    frame.header.width = 16;
    frame.header.height = 12;
    frame.image = QImage(
        16,
        12,
        QImage::Format_RGB888);
    frame.image.fill(color);
    return frame;
}

}

void LatestCameraFrameBufferTest::startsEmpty()
{
    LatestCameraFrameBuffer buffer;

    QVERIFY(!buffer.hasPendingFrame());
    QCOMPARE(buffer.replacedFrameCount(), 0ULL);
    QVERIFY(!buffer.take().has_value());
}

void LatestCameraFrameBufferTest::
    storesAndTakesSingleFrame()
{
    LatestCameraFrameBuffer buffer;
    DecodedCameraFrame sourceFrame =
        makeFrame(101, QColor(20, 40, 60));

    const std::optional<quint64> replacedSequence =
        buffer.submit(std::move(sourceFrame));

    QVERIFY(!replacedSequence.has_value());
    QVERIFY(buffer.hasPendingFrame());
    QCOMPARE(buffer.replacedFrameCount(), 0ULL);

    std::optional<DecodedCameraFrame> takenFrame =
        buffer.take();

    QVERIFY(takenFrame.has_value());
    QCOMPARE(takenFrame->header.sequence, 101ULL);
    QCOMPARE(takenFrame->image.size(), QSize(16, 12));
    QCOMPARE(
        takenFrame->image.pixelColor(0, 0),
        QColor(20, 40, 60));
    QVERIFY(!buffer.hasPendingFrame());
    QVERIFY(!buffer.take().has_value());
}

void LatestCameraFrameBufferTest::
    replacesPendingFrameWithLatest()
{
    LatestCameraFrameBuffer buffer;

    QVERIFY(!buffer.submit(
        makeFrame(201, QColor(10, 20, 30)))
                 .has_value());

    const std::optional<quint64> firstReplaced =
        buffer.submit(
            makeFrame(202, QColor(40, 50, 60)));
    QVERIFY(firstReplaced.has_value());
    QCOMPARE(*firstReplaced, 201ULL);
    QCOMPARE(buffer.replacedFrameCount(), 1ULL);

    const std::optional<quint64> secondReplaced =
        buffer.submit(
            makeFrame(203, QColor(70, 80, 90)));
    QVERIFY(secondReplaced.has_value());
    QCOMPARE(*secondReplaced, 202ULL);
    QCOMPARE(buffer.replacedFrameCount(), 2ULL);

    const std::optional<DecodedCameraFrame> latestFrame =
        buffer.take();

    QVERIFY(latestFrame.has_value());
    QCOMPARE(latestFrame->header.sequence, 203ULL);
    QCOMPARE(
        latestFrame->image.pixelColor(0, 0),
        QColor(70, 80, 90));
    QVERIFY(!buffer.hasPendingFrame());
    QCOMPARE(buffer.replacedFrameCount(), 2ULL);
}

void LatestCameraFrameBufferTest::
    resetClearsFrameAndStatistics()
{
    LatestCameraFrameBuffer buffer;

    buffer.submit(
        makeFrame(301, QColor(1, 2, 3)));
    buffer.submit(
        makeFrame(302, QColor(4, 5, 6)));

    QVERIFY(buffer.hasPendingFrame());
    QCOMPARE(buffer.replacedFrameCount(), 1ULL);

    buffer.reset();

    QVERIFY(!buffer.hasPendingFrame());
    QCOMPARE(buffer.replacedFrameCount(), 0ULL);
    QVERIFY(!buffer.take().has_value());
}

QTEST_GUILESS_MAIN(LatestCameraFrameBufferTest)

#include "LatestCameraFrameBufferTest.moc"
