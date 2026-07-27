#include "ImageWorker.h"

ImageWorker::ImageWorker(
    QString modelPath,
    InferenceOptions options,
    QObject* parent)
    : QObject(parent)
    , modelPath_(std::move(modelPath))
    , options_(options)
{
}

void ImageWorker::initialize()
{
    QString errorMessage;
    if(!inferenceEngine_.loadModel(modelPath_,&errorMessage))
    {
        emit sigInitializationFailed(errorMessage);
        return ;
    }

    ready_ = true;
    emit sigReady();
}

void ImageWorker::processItem(const ClaimedInspectionItem& item)
{
    if (!ready_)
    {
        emit sigTaskFailed(
            item,
            QStringLiteral("Inference Worker is not Ready."));
        return;
    }

    const QImage sourceImage(item.sourceFilePath);
    if (sourceImage.isNull())
    {
        emit sigTaskFailed(
            item,
            QStringLiteral("Failed to Load Source Image."));
        return;
    }

    QString errorMessage;
    InferenceResult result;
    if (!inferenceEngine_.inferImage(
            sourceImage,
            options_,
            &result,
            &errorMessage))
    {
        emit sigTaskFailed(item, errorMessage);
        return;
    }

    emit sigTaskFinished(item, result);
}
