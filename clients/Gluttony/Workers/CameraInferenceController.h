#ifndef CAMERAINFERENCECONTROLLER_H
#define CAMERAINFERENCECONTROLLER_H

#include <QObject>
#include <QPointer>
#include <QString>
#include <QThread>
#include <QMetaType>

#include <utility>

#include "CameraInferenceWorker.h"

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

signals:
    void sigReady();
    void sigInitializationFailed(const QString& errorMessage);
    void sigStopped();
private:
    QThread* workerThread_ = nullptr;
    QPointer<CameraInferenceWorker> worker_;
    bool ready_ = false;
};












#endif
