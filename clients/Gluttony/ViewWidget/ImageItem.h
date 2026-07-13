#ifndef IMAGEITEM_H
#define IMAGEITEM_H

#include <QImage>
#include <QGraphicsPixmapItem>
#include <QObject>

class ImageItem : public QObject, public QGraphicsPixmapItem
{
    Q_OBJECT
public:
    explicit ImageItem(QWidget* parent = nullptr);
    void setW(int OutW);
    void setH(int OutH);
protected:
    void hoverMoveEvent(QGraphicsSceneHoverEvent* event) override;
signals:
    void ImageInfo(QString imageInfo);
private:
    int w;
    int h;
};







#endif
