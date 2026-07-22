#include "ImageQueue.h"


ImageQueue::ImageQueue()
{
    maxSize_ = 10;
    shutdownFlag_ = false;
    for(int i = 0; i < maxSize_; i++)
    {
        freeQueue_.push(ImageTask());
    }
}

ImageQueue::ImageQueue(size_t maxSize)
{
    maxSize_ = maxSize == 0 ? 10 : maxSize;
    shutdownFlag_ = false;
    for(size_t i = 0; i < maxSize_; i++)
    {
        freeQueue_.push(ImageTask());
    }
}

ImageQueue::~ImageQueue()
{
    shutdown();
}

ImageQueue::ImageQueue(ImageQueue&& other) noexcept
{
    std::lock_guard<std::mutex> lock(other.mutex_);
    freeQueue_ = std::move(other.freeQueue_);
    workQueue_ = std::move(other.workQueue_);
    maxSize_ = std::move(other.maxSize_);
    shutdownFlag_ = other.shutdownFlag_.load();
}

ImageQueue& ImageQueue::operator=(ImageQueue&& other) noexcept
{

    if(this != &other)
    {
        std::scoped_lock lock(mutex_,other.mutex_);

        freeQueue_ = std::move(other.freeQueue_);
        workQueue_ = std::move(other.workQueue_);
        maxSize_ = std::move(other.maxSize_);
        shutdownFlag_ = other.shutdownFlag_.load();

        notEmptyCondition_.notify_all();
        notFullCondition_.notify_all();
    }
    return *this;
}

bool ImageQueue::push(const ImageTask& task, int timeoutMs)
{
    std::unique_lock<std::mutex> lock(mutex_);
    if(shutdownFlag_)
    {
        QWARNING("Push Image Failed Because the Queue is ShuttingDown");
        return false;
    }
    if(!waitForNotFull(lock,timeoutMs))
    {
        QWARNING(QString("Push Image Failed Because the Queue is Still Full After %1 Ms").arg(timeoutMs));
        return false;
    }
    bool wasEmpty = workQueue_.empty();
    if(!freeQueue_.empty())
    {
        ImageTask temp = freeQueue_.front();
        freeQueue_.pop();
        temp = task;
        workQueue_.push(temp);
    }
    else
    {
        QWARNING("Push Task Failed Because the Free Queue is Empty");
        return false;
    }
    QDEBUG("Push Image Successfully!");
    lock.unlock();
    if(wasEmpty)
    {
        notEmptyCondition_.notify_one();
    }
    return true;
}

bool ImageQueue::pop(ImageTask& task, int timeoutMs)
{
    std::unique_lock<std::mutex> lock(mutex_);
    if(shutdownFlag_)
    {
        QWARNING("Pop Image Failed Because the Queue is ShuttingDown");
        return false;
    }
    if(!waitForNotEmpty(lock,timeoutMs))
    {
        QWARNING(QString("Pop Image Failed Because the Queue is Still Full After %1 Ms").arg(timeoutMs));
        return false;
    }
    bool wasFull = workQueue_.size() >= maxSize_;
    task = workQueue_.front();
    workQueue_.pop();
    freeQueue_.push(task);
    QDEBUG("Pop Image Successfully!");
    lock.unlock();

    if(wasFull)
    {
        notFullCondition_.notify_one();
    }
    return true;
}

void ImageQueue::clear()
{
    std::lock_guard<std::mutex> lock(mutex_);
    while(!workQueue_.empty())
    {
        workQueue_.pop();
        freeQueue_.push(ImageTask{});
    }
    notFullCondition_.notify_all();
    QDEBUG("Clear Image Successfully!");
}

bool ImageQueue::isEmpty()
{
    return workQueue_.empty();
}

bool ImageQueue::isFull()
{
    return workQueue_.size() >= maxSize_;
}

void ImageQueue::shutdown()
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        shutdownFlag_ = true;
        QDEBUG("Queue is ShuttingDown!");
    }
    notEmptyCondition_.notify_all();
    notFullCondition_.notify_all();
    resumeCondition_.notify_all();
}

size_t ImageQueue::maxSize()
{
    return maxSize_;
}

size_t ImageQueue::size()
{
    return workQueue_.size();
}

bool ImageQueue::waitForNotFull(std::unique_lock<std::mutex>& lock, int timeoutMs)
{
    if(shutdownFlag_)
    {
        return false;
    }
    if(workQueue_.size() < maxSize_)
    {
        return true;
    }
    if(timeoutMs <= 0)
    {
        notFullCondition_.wait(lock,[this](){
            return shutdownFlag_ || workQueue_.size() < maxSize_;
        });
        return !shutdownFlag_ && workQueue_.size() < maxSize_;
    }
    else
    {
        auto notReachTimeout = notFullCondition_.wait_for(lock,std::chrono::milliseconds(timeoutMs),[this](){
           return shutdownFlag_ || workQueue_.size() < maxSize_;
        });
        return !shutdownFlag_ && workQueue_.size() < maxSize_ && notReachTimeout;
    }
}


bool ImageQueue::waitForNotEmpty(std::unique_lock<std::mutex>& lock, int timeoutMs)
{
    if(shutdownFlag_)
    {
        return false;
    }
    if(!workQueue_.empty())
    {
        return true;
    }
    if(timeoutMs <= 0)
    {
        notEmptyCondition_.wait(lock,[this](){
            return shutdownFlag_ || !workQueue_.empty();
        });
        return !shutdownFlag_ && !workQueue_.empty();
    }
    else
    {
        auto notReachTimeout = notEmptyCondition_.wait_for(lock,std::chrono::milliseconds(timeoutMs),[this](){
            return shutdownFlag_ || !workQueue_.empty();
        });
        return !shutdownFlag_ && !workQueue_.empty() && notReachTimeout;
    }
}


void ImageQueue::pause()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if(!shutdownFlag_)
    {
        isPaused_ = true;
    }
}

void ImageQueue::resume()
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        isPaused_ = false;
    }
    resumeCondition_.notify_all();
}


bool ImageQueue::isPaused()
{
    std::lock_guard<std::mutex> lock(mutex_);
    return isPaused_;
}


bool ImageQueue::waitForResume()
{
    std::unique_lock<std::mutex> lock(mutex_);
    if(shutdownFlag_)
    {
        return false;
    }
    if(!isPaused_)
    {
        return true;
    }
    resumeCondition_.wait(lock,[this](){
        return shutdownFlag_ || !isPaused_;
    });
    return !shutdownFlag_ && !isPaused_;
}
