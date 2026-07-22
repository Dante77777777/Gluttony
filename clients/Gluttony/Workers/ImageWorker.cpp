#include "ImageWorker.h"

ImageWorker::ImageWorker(std::shared_ptr<ImageQueue> queue, const QString& modelPath, QObject* parent)
    : QObject(parent)
    , queue_(std::move(queue))
    , modelPath_(modelPath)
{

}

void ImageWorker::run()
{
    QString errorMessage;
    if(!inferenceEngine_.loadModel(modelPath_,&errorMessage))
    {
        emit sigTaskFailed(modelPath_,errorMessage);
        emit sigFinished();
        return ;
    }

    while(true)
    {
        if(queue_->isPaused())
        {
            emit sigPaused();
            if(!queue_->waitForResume())
            {
                break;
            }
        }
        ImageTask task;
        if(!queue_->pop(task,0))
        {
            break;
        }

        if(task.isEnd)
        {
            break;
        }

        const QImage sourceImage = ImageConver::cvMat2QImage(task.image);
        if(sourceImage.isNull())
        {
            emit sigTaskFailed(task.filePath, "Failed to Convert cv::Mat to QImage.");
            continue;
        }

        InferenceResult result;
        if(!inferenceEngine_.inferImage(sourceImage,&result,&errorMessage))
        {
            emit sigTaskFailed(task.filePath,errorMessage);
            continue ;
        }

        emit sigTaskFinished(task.filePath,sourceImage,result);
    }

    emit sigFinished();
}
