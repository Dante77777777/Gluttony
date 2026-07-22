#ifndef IMAGEPRODUCER_H
#define IMAGEPRODUCER_H

#include <QStringList>
#include <QObject>
#include <memory>
#include <QDirIterator>
#include <QFileInfo>
#include "../Utils/ImageQueue.h"


class ImageProducer : public QObject
{
    Q_OBJECT

public:
    explicit ImageProducer(std::shared_ptr<ImageQueue> queue, QObject* parent = nullptr);

    void produceFiles(const QStringList& filePaths);
    void produceFolder(const QString& folderPath);

signals:
    void sigTotalReady(int total);
    void sigImageReadFailed(const QString& filePath);
    void sigFinish();

private:
    std::shared_ptr<ImageQueue> queue_;
};

#endif
