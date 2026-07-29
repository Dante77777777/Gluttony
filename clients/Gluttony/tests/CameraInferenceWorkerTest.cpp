#include <QImage>
#include <QSignalSpy>
#include <QThread>
#include <QtTest>

#include "Workers/CameraInferenceWorker.h"

class CameraInferenceWorkerTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void rejectsFrameBeforeInitialization();
    void reportsInvalidModelInitialization();
    void processesQueuedFrameOnWorkerThread();
};

namespace
{

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
    frame.header.width = 32;
    frame.header.height = 24;
    frame.image = QImage(
        32,
        24,
        QImage::Format_RGB888);
    frame.image.fill(QColor(24, 80, 136));
    return frame;
}

}

void CameraInferenceWorkerTest::initTestCase()
{
    qRegisterMetaType<DecodedCameraFrame>(
        "DecodedCameraFrame");
    qRegisterMetaType<InferenceResult>(
        "InferenceResult");
}

void CameraInferenceWorkerTest::
    rejectsFrameBeforeInitialization()
{
    CameraInferenceWorker worker(
        QStringLiteral("unused-model.xml"),
        InferenceOptions{});
    QSignalSpy failedSpy(
        &worker,
        &CameraInferenceWorker::sigFrameFailed);
    QSignalSpy finishedSpy(
        &worker,
        &CameraInferenceWorker::sigFrameFinished);

    worker.processFrame(makeFrame(1001));

    QCOMPARE(failedSpy.count(), 1);
    QCOMPARE(
        failedSpy.first().at(0).toULongLong(),
        1001ULL);
    QVERIFY(
        failedSpy.first().at(1).toString()
            .contains(
                QStringLiteral("not ready"),
                Qt::CaseInsensitive));
    QCOMPARE(finishedSpy.count(), 0);
}

void CameraInferenceWorkerTest::
    reportsInvalidModelInitialization()
{
    CameraInferenceWorker worker(
        QStringLiteral(
            "Z:/missing/gluttony-model.xml"),
        InferenceOptions{});
    QSignalSpy readySpy(
        &worker,
        &CameraInferenceWorker::sigReady);
    QSignalSpy initializationFailedSpy(
        &worker,
        &CameraInferenceWorker::
            sigInitializationFailed);

    worker.initialize();

    QCOMPARE(readySpy.count(), 0);
    QCOMPARE(initializationFailedSpy.count(), 1);
    QVERIFY(
        !initializationFailedSpy.first()
             .at(0)
             .toString()
             .isEmpty());
}

void CameraInferenceWorkerTest::
    processesQueuedFrameOnWorkerThread()
{
    QThread workerThread;
    auto* worker = new CameraInferenceWorker(
        QStringLiteral("unused-model.xml"),
        InferenceOptions{});
    worker->moveToThread(&workerThread);

    connect(
        &workerThread,
        &QThread::finished,
        worker,
        &QObject::deleteLater);

    QSignalSpy failedSpy(
        worker,
        &CameraInferenceWorker::sigFrameFailed);

    workerThread.start();
    QCOMPARE(worker->thread(), &workerThread);

    const DecodedCameraFrame frame =
        makeFrame(2002);

    QVERIFY(QMetaObject::invokeMethod(
        worker,
        [worker, frame]()
        {
            worker->processFrame(frame);
        },
        Qt::QueuedConnection));

    QTRY_COMPARE_WITH_TIMEOUT(
        failedSpy.count(),
        1,
        2000);
    QCOMPARE(
        failedSpy.first().at(0).toULongLong(),
        2002ULL);

    workerThread.quit();
    QVERIFY(workerThread.wait(2000));
}

QTEST_GUILESS_MAIN(CameraInferenceWorkerTest)

#include "CameraInferenceWorkerTest.moc"
