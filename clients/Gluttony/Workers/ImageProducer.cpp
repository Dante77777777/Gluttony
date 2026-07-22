#include "ImageProducer.h"


ImageProducer::ImageProducer(std::shared_ptr<ImageQueue> queue, QObject* parent)
    : QObject(parent)
    , queue_(std::move(queue))
{

}


void ImageProducer::produceFiles(const QStringList& filePaths)
{
    emit sigTotalReady(filePaths.size());

    for(const auto& file : filePaths)
    {
        if(!queue_->waitForResume())
        {
            break ;
        }

        cv::Mat image = cv::imread(file.toStdString(),cv::IMREAD_COLOR);

        if(image.empty())
        {
            emit sigImageReadFailed(file);
            continue ;
        }

        if(!queue_->waitForResume())
        {
            break ;
        }

        ImageTask task;
        task.image = image;
        task.filePath = file;
        task.isEnd = false;

        if(!queue_->push(task,0))
        {
            break;
        }
    }
    ImageTask endTask;
    endTask.isEnd = true;
    queue_->push(endTask,0);

    emit sigFinish();
}


void ImageProducer::produceFolder(const QString& folderPath)
{
    QStringList filePaths;
    QDirIterator iterator(
        folderPath,
        {"*png", "*.jpg", "*.jpeg", "*.bmp"},
        QDir::Files,
        QDirIterator::Subdirectories
    );

    while(iterator.hasNext())
    {
        filePaths.append(iterator.next());
    }

    produceFiles(filePaths);
}
