#ifndef CAMERAINFERENCEWORKER_H
#define CAMERAINFERENCEWORKER_H

#include <QObject>
#include <QString>

#include <utility>

#include "../Inference/InferenceEngine.h"
#include "../Transport/ReceivedFrameImageDecoder.h"

class CameraInferenceWorker final : public QObject
{
    Q_OBJECT

public:
    explicit CameraInferenceWorker(QString modelPath, InferenceOptions options, QObject* parent = nullptr);

public slots:
    void initialize();

    void processFrame(const DecodedCameraFrame& frame);

signals:
    void sigReady();

    void sigInitializationFailed(const QString& errorMessage);

    void sigFrameFinished(const DecodedCameraFrame& frame, const InferenceResult& result);

    void sigFrameFailed(quint64 sequence, const QString& errorMessage);

private:
    QString modelPath_;
    InferenceEngine inferenceEngine_;
    InferenceOptions options_;
    bool ready_ = false;
};











#endif
