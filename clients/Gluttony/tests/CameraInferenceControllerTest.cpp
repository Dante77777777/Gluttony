#include <QSignalSpy>
#include <QtTest>

#include <memory>
#include <utility>

#include "Workers/CameraInferenceController.h"

class CameraInferenceControllerTest : public QObject
{
    Q_OBJECT

private slots:
    void startsThreadAndReportsInvalidModel();
    void rejectsFrameWhenNotRunning();
    void keepsOnlyLatestFrameWhileNotReady();
    void stopIsIdempotent();
    void destructorStopsRunningThread();
};

namespace
{

QString missingModelPath()
{
    return QStringLiteral(
        "Z:/missing/gluttony-camera-model.xml");
}

DecodedCameraFrame makeFrame(
    quint64 sequence)
{
    DecodedCameraFrame frame;
    frame.header.messageType =
        FrameProtocol::MessageType::ImageFrame;
    frame.header.pixelFormat =
        FrameProtocol::PixelFormat::Jpeg;
    frame.header.sequence = sequence;
    frame.header.timestampMs =
        1'785'211'200'000ULL + sequence;
    frame.header.width = 24;
    frame.header.height = 16;
    frame.image = QImage(
        24,
        16,
        QImage::Format_RGB888);
    frame.image.fill(QColor(40, 80, 120));
    return frame;
}

}

void CameraInferenceControllerTest::
    startsThreadAndReportsInvalidModel()
{
    CameraInferenceController controller;
    QSignalSpy readySpy(
        &controller,
        &CameraInferenceController::sigReady);
    QSignalSpy initializationFailedSpy(
        &controller,
        &CameraInferenceController::
            sigInitializationFailed);
    QSignalSpy stoppedSpy(
        &controller,
        &CameraInferenceController::sigStopped);

    controller.start(
        missingModelPath(),
        InferenceOptions{});

    QTRY_VERIFY_WITH_TIMEOUT(
        controller.isRunning(),
        2000);
    QTRY_COMPARE_WITH_TIMEOUT(
        initializationFailedSpy.count(),
        1,
        2000);

    QVERIFY(!controller.isReady());
    QCOMPARE(readySpy.count(), 0);
    QVERIFY(
        !initializationFailedSpy.first()
             .at(0)
             .toString()
             .isEmpty());

    controller.stop();

    QVERIFY(!controller.isRunning());
    QVERIFY(!controller.isReady());
    QCOMPARE(stoppedSpy.count(), 1);
}

void CameraInferenceControllerTest::
    rejectsFrameWhenNotRunning()
{
    CameraInferenceController controller;
    QSignalSpy failedSpy(
        &controller,
        &CameraInferenceController::sigFrameFailed);
    QSignalSpy droppedSpy(
        &controller,
        &CameraInferenceController::sigFrameDropped);

    controller.submitFrame(makeFrame(3001));

    QCOMPARE(failedSpy.count(), 1);
    QCOMPARE(
        failedSpy.first().at(0).toULongLong(),
        3001ULL);
    QVERIFY(
        failedSpy.first().at(1).toString()
            .contains(
                QStringLiteral("not running"),
                Qt::CaseInsensitive));
    QCOMPARE(droppedSpy.count(), 0);
    QVERIFY(!controller.isBusy());
    QVERIFY(!controller.hasPendingFrame());
    QCOMPARE(controller.droppedFrameCount(), 0ULL);
}

void CameraInferenceControllerTest::
    keepsOnlyLatestFrameWhileNotReady()
{
    CameraInferenceController controller;
    QSignalSpy initializationFailedSpy(
        &controller,
        &CameraInferenceController::
            sigInitializationFailed);
    QSignalSpy failedSpy(
        &controller,
        &CameraInferenceController::sigFrameFailed);
    QSignalSpy droppedSpy(
        &controller,
        &CameraInferenceController::sigFrameDropped);

    controller.start(
        missingModelPath(),
        InferenceOptions{});

    QTRY_COMPARE_WITH_TIMEOUT(
        initializationFailedSpy.count(),
        1,
        2000);
    QVERIFY(controller.isRunning());
    QVERIFY(!controller.isReady());

    controller.submitFrame(makeFrame(4001));
    QVERIFY(controller.hasPendingFrame());
    QCOMPARE(controller.droppedFrameCount(), 0ULL);

    controller.submitFrame(makeFrame(4002));
    controller.submitFrame(makeFrame(4003));

    QCOMPARE(droppedSpy.count(), 2);
    QCOMPARE(
        droppedSpy.at(0).at(0).toULongLong(),
        4001ULL);
    QCOMPARE(
        droppedSpy.at(0).at(1).toULongLong(),
        1ULL);
    QCOMPARE(
        droppedSpy.at(1).at(0).toULongLong(),
        4002ULL);
    QCOMPARE(
        droppedSpy.at(1).at(1).toULongLong(),
        2ULL);

    QCOMPARE(failedSpy.count(), 0);
    QVERIFY(!controller.isBusy());
    QVERIFY(controller.hasPendingFrame());
    QCOMPARE(controller.droppedFrameCount(), 2ULL);

    controller.stop();

    QVERIFY(!controller.hasPendingFrame());
    QVERIFY(!controller.isBusy());
    QCOMPARE(controller.droppedFrameCount(), 0ULL);
}

void CameraInferenceControllerTest::stopIsIdempotent()
{
    CameraInferenceController controller;
    QSignalSpy initializationFailedSpy(
        &controller,
        &CameraInferenceController::
            sigInitializationFailed);
    QSignalSpy stoppedSpy(
        &controller,
        &CameraInferenceController::sigStopped);

    controller.stop();
    QCOMPARE(stoppedSpy.count(), 0);

    controller.start(
        missingModelPath(),
        InferenceOptions{});

    QTRY_COMPARE_WITH_TIMEOUT(
        initializationFailedSpy.count(),
        1,
        2000);

    controller.stop();
    QCOMPARE(stoppedSpy.count(), 1);

    controller.stop();
    QCOMPARE(stoppedSpy.count(), 1);
    QVERIFY(!controller.isRunning());
    QVERIFY(!controller.isReady());
}

void CameraInferenceControllerTest::
    destructorStopsRunningThread()
{
    auto controller =
        std::make_unique<CameraInferenceController>();
    QSignalSpy initializationFailedSpy(
        controller.get(),
        &CameraInferenceController::
            sigInitializationFailed);

    controller->start(
        missingModelPath(),
        InferenceOptions{});

    QTRY_VERIFY_WITH_TIMEOUT(
        controller->isRunning(),
        2000);
    QTRY_COMPARE_WITH_TIMEOUT(
        initializationFailedSpy.count(),
        1,
        2000);

    controller.reset();

    QVERIFY(true);
}

QTEST_GUILESS_MAIN(CameraInferenceControllerTest)

#include "CameraInferenceControllerTest.moc"
