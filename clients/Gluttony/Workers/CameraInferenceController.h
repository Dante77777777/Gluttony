#ifndef CAMERAINFERENCECONTROLLER_H
#define CAMERAINFERENCECONTROLLER_H

#include <QObject>
#include <QPointer>
#include <QString>
#include <QThread>
#include <QMetaType>

#include <utility>

#include "CameraInferenceWorker.h"
#include "LatestCameraFrameBuffer.h"

class CameraInferenceController final : public QObject
{
    Q_OBJECT

public:
    explicit CameraInferenceController(QObject* parent = nullptr);

    ~CameraInferenceController() override;

    void start(QString modelPath, InferenceOptions options);

    void stop();

    bool isRunning() const;
    bool isReady() const noexcept;
    bool isBusy() const noexcept;
    bool hasPendingFrame() const noexcept;

    quint64 droppedFrameCount() const noexcept;

public slots:
    void submitFrame(DecodedCameraFrame frame);

signals:
    void sigReady();
    void sigInitializationFailed(const QString& errorMessage);
    void sigFrameFinished(const DecodedCameraFrame& frame, const InferenceResult& result);
    void sigFrameFailed(quint64 sequence, const QString& errorMessage);
    void sigFrameDropped(quint64 sequence, quint64 totalDroppedCount);
    void sigStopped();

private:
    void dispatchFrame(DecodedCameraFrame frame);
    void dispatchNextPendingFrame();

private:
    QThread* workerThread_ = nullptr;
    QPointer<CameraInferenceWorker> worker_;

    LatestCameraFrameBuffer frameBuffer_;

    bool ready_ = false;
    bool busy_ = false;

    quint64 sessionGeneration_ = 0;
};


#endif
