#include "ImageItem.h"
#include <QGraphicsSceneHoverEvent>

ImageItem::ImageItem(QWidget* parent) : QGraphicsPixmapItem(nullptr)
{
    setAcceptHoverEvents(true);
};

void ImageItem::hoverMoveEvent(QGraphicsSceneHoverEvent* event)
{
    QPointF pos = event->pos();

    int x = pos.x();
    int y = pos.y();
    int R, G, B;
    if(x < 0)
    {
        x = 0;
    }
    if(y < 0)
    {
        y = 0;
    }
    pixmap().toImage().pixelColor(x,y).getRgb(&R,&G,&B);
    QString imageInfo = QString("W:%1,H:%2 | X:%3,Y:%4 | R:%5,G:%6,B:%7")
                                .arg(QString::number(w))
                                .arg(QString::number(h))
                                .arg(QString::number(x))
                                .arg(QString::number(y))
                                .arg(QString::number(R))
                                .arg(QString::number(G))
                                .arg(QString::number(B));
}


void ImageItem::setW(int OutW)
{
    w = OutW;
}


void ImageItem::setH(int OutH)
{
    h = OutH;
}
