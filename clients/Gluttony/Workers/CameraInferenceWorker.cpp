#include "CameraInferenceWorker.h"

CameraInferenceWorker::CameraInferenceWorker(QString modelPath, InferenceOptions options, QObject* parent)
    : QObject(parent)
    , modelPath_(std::move(modelPath))
    , options_(options)
{

}


void CameraInferenceWorker::initialize()
{
    QString errorMessage;
    if(!inferenceEngine_.loadModel(modelPath_,&errorMessage))
    {
        ready_ = false;
        emit sigInitializationFailed(errorMessage);
        return ;
    }

    ready_ = true;
    emit sigReady();
}


void CameraInferenceWorker::processFrame(const DecodedCameraFrame& frame)
{
    if(!ready_)
    {
        emit sigFrameFailed(frame,QStringLiteral("Camera inference worker is not ready"));
        return ;
    }

    if(frame.image.isNull())
    {
        emit sigFrameFailed(frame,QStringLiteral("Camera frame image is empty"));
        return ;
    }

    InferenceResult result;
    QString errorMessage;

    if(!inferenceEngine_.inferImage(frame.image,options_,&result,&errorMessage))
    {
        emit sigFrameFailed(frame,errorMessage);
        return ;
    }

    emit sigFrameFinished(frame,result);
}
