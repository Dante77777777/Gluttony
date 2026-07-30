#include "CameraInferenceController.h"

CameraInferenceController::CameraInferenceController(QObject* parent)
    : QObject(parent)
{
    qRegisterMetaType<DecodedCameraFrame>("DecodedCameraFrame");
    qRegisterMetaType<InferenceResult>("InferenceResult");
}


CameraInferenceController::~CameraInferenceController()
{
    stop();
}


void CameraInferenceController::start(QString modelPath, InferenceOptions options)
{
    stop();
    ready_ = false;
    workerThread_ = new QThread(this);
    worker_ = new CameraInferenceWorker(std::move(modelPath),options);
    worker_->moveToThread(workerThread_);

    connect(workerThread_,&QThread::started,worker_,&CameraInferenceWorker::initialize);

    connect(workerThread_,&QThread::finished,worker_,&QObject::deleteLater);

    connect(worker_,&CameraInferenceWorker::sigReady,this,[this](){
        ready_ = true;
        emit sigReady();
    });

    connect(worker_,&CameraInferenceWorker::sigInitializationFailed,this,[this](const QString& errorMessage){
        ready_ = false;
        emit sigInitializationFailed(errorMessage);
    });

    workerThread_->start();
}

void CameraInferenceController::stop()
{
    ready_ = false;

    if(workerThread_ == nullptr)
    {
        worker_.clear();
        return ;
    }

    if(workerThread_->isRunning())
    {
        workerThread_->quit();
        workerThread_->wait();
    }

    worker_.clear();

    delete workerThread_;
    workerThread_ = nullptr;
    emit sigStopped();
}

bool CameraInferenceController::isRunning() const
{
    return workerThread_ != nullptr && workerThread_->isRunning();
}

bool CameraInferenceController::isReady() const noexcept
{
    return ready_;
}
