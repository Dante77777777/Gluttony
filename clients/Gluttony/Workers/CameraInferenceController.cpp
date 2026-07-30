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
    busy_ = false;
    frameBuffer_.reset();

    workerThread_ = new QThread(this);
    worker_ = new CameraInferenceWorker(std::move(modelPath),options);
    worker_->moveToThread(workerThread_);

    const quint64 generation = sessionGeneration_;

    connect(workerThread_,&QThread::started,worker_,&CameraInferenceWorker::initialize);

    connect(workerThread_,&QThread::finished,worker_,&QObject::deleteLater);

    connect(worker_,&CameraInferenceWorker::sigReady,this,[this, generation](){
        if(generation != sessionGeneration_)
        {
            return ;
        }

        ready_ = true;
        emit sigReady();
        dispatchNextPendingFrame();
    });

    connect(worker_,&CameraInferenceWorker::sigInitializationFailed,this,[this, generation](const QString& errorMessage){
        if(generation != sessionGeneration_)
        {
            return ;
        }
        ready_ = false;
        emit sigInitializationFailed(errorMessage);
    });

    connect(worker_,&CameraInferenceWorker::sigFrameFinished,this,[this, generation](const DecodedCameraFrame& frame, const InferenceResult& result){
        if(generation != sessionGeneration_)
        {
            return ;
        }
        busy_ = false;

        emit sigFrameFinished(frame,result);

        dispatchNextPendingFrame();
    });

    connect(worker_,&CameraInferenceWorker::sigFrameFailed,this,[this, generation](quint64 sequence, const QString& errorMessage){
        if(generation != sessionGeneration_)
        {
            return ;
        }

        busy_ = false;
        emit sigFrameFailed(sequence,errorMessage);
        dispatchNextPendingFrame();
    });

    workerThread_->start();
}

void CameraInferenceController::stop()
{
    ++sessionGeneration_;
    ready_ = false;
    busy_ = false;
    frameBuffer_.reset();

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


void CameraInferenceController::submitFrame(DecodedCameraFrame frame)
{
    const quint64 sequence = frame.header.sequence;
    if(!isRunning() || worker_.isNull())
    {
        emit sigFrameFailed(sequence,QStringLiteral("Camera inference controller is not running"));
        return ;
    }

    if(!ready_ || busy_)
    {
        const std::optional<quint64> replacedSequence = frameBuffer_.submit(std::move(frame));
        if(replacedSequence.has_value())
        {
            emit sigFrameDropped(*replacedSequence,frameBuffer_.replacedFrameCount());
        }
        return ;
    }
    dispatchFrame(std::move(frame));
}


void CameraInferenceController::dispatchFrame(DecodedCameraFrame frame)
{
    const quint64 sequence = frame.header.sequence;
    if(!ready_ || busy_ || worker_.isNull())
    {
        emit sigFrameFailed(sequence,QStringLiteral("Camera inference worker is unavailable"));
        return ;
    }

    CameraInferenceWorker* worker = worker_.data();
    busy_ = true;

    const bool queued = QMetaObject::invokeMethod(worker,[worker, frame = std::move(frame)]() mutable{
        worker->processFrame(frame);
    },Qt::QueuedConnection);

    if(!queued)
    {
        busy_ = false;
        emit sigFrameFailed(sequence,QStringLiteral("Failed to queue camera inference frame"));
        dispatchNextPendingFrame();
    }
}


void CameraInferenceController::dispatchNextPendingFrame()
{
    if(!ready_ || busy_)
    {
        return ;
    }

    std::optional<DecodedCameraFrame> nextFrame = frameBuffer_.take();
    if(!nextFrame.has_value())
    {
        return ;
    }
    dispatchFrame(std::move(*nextFrame));
}


bool CameraInferenceController::isRunning() const
{
    return workerThread_ != nullptr && workerThread_->isRunning();
}

bool CameraInferenceController::isBusy() const noexcept
{
    return busy_;
}

bool CameraInferenceController::hasPendingFrame() const noexcept
{
    return frameBuffer_.hasPendingFrame();
}

quint64 CameraInferenceController::droppedFrameCount() const noexcept
{
    return frameBuffer_.replacedFrameCount();
}

bool CameraInferenceController::isReady() const noexcept
{
    return ready_;
}
