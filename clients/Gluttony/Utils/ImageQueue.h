#ifndef IMAGEQUEUE_H
#define IMAGEQUEUE_H

#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include <QString>
#include "opencv2/opencv.hpp"
#include "../LogWidget/LogMessage.h"

struct ImageTask
{
    QString filePath;
    cv::Mat image;
    bool isEnd = false;
};


class ImageQueue
{
public:
    ImageQueue();
    ImageQueue(size_t maxSize);
    ~ImageQueue();

    ImageQueue(const ImageQueue&)=delete;
    ImageQueue& operator=(const ImageQueue&)=delete;

    ImageQueue(ImageQueue&& other) noexcept;
    ImageQueue& operator=(ImageQueue&& other) noexcept;

    bool push(const ImageTask& task, int timeoutMs);
    bool pop(ImageTask& task, int timeoutMs);

    void clear();
    bool isEmpty();
    bool isFull();
    void shutdown();

    void pause();
    void resume();
    bool isPaused();
    bool waitForResume();

    size_t maxSize();
    size_t size();
private:
    std::mutex mutex_;
    std::queue<ImageTask> freeQueue_;
    std::queue<ImageTask> workQueue_;
    size_t maxSize_;

    std::atomic<bool> shutdownFlag_;
    bool isPaused_ = false;
    std::condition_variable notFullCondition_;
    std::condition_variable notEmptyCondition_;
    std::condition_variable resumeCondition_;

    bool waitForNotFull(std::unique_lock<std::mutex>& lock, int timeoutMs = 0);
    bool waitForNotEmpty(std::unique_lock<std::mutex>& lock, int timeoutMs = 0);

};







#endif
