#include <QSignalSpy>
#include <QtTest>

#include <memory>

#include "Workers/CameraInferenceController.h"

class CameraInferenceControllerTest : public QObject
{
    Q_OBJECT

private slots:
    void startsThreadAndReportsInvalidModel();
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
