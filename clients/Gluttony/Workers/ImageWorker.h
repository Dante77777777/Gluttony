#ifndef IMAGEWORKER_H
#define IMAGEWORKER_H

#include <QStringList>
#include <QObject>
#include <memory>

#include "../Utils/ImageQueue.h"
#include "../Inference/InferenceEngine.h"
#include "../Utils/ImageConversion.h"

class ImageWorker : public QObject
{
    Q_OBJECT

public:
    explicit ImageWorker(std::shared_ptr<ImageQueue> queue, const QString& modelPath, const InferenceOptions& options, QObject* parent = nullptr);

public slots:
    void run();

signals:
    void sigTaskFinished(const QString& filePath, const QImage& sourceImage, const InferenceResult& result);
    void sigFinished();
    void sigTaskFailed(const QString& filePath, const QString& errorMessage);
    void sigPaused();
private:
    QString modelPath_;
    std::shared_ptr<ImageQueue> queue_;
    InferenceEngine inferenceEngine_;
    InferenceOptions options_;
};





#endif
