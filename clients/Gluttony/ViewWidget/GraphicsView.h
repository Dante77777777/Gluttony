#ifndef GRAPHICSVIEW_H
#define GRAPHICSVIEW_H

#include "ImageItem.h"
#include <QGraphicsView>
#include <QLabel>
#include <QBoxLayout>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QResizeEvent>
#include <QPaintEvent>
#include <QImage>
#include <QMutex>

class GraphicsView : public QGraphicsView
{
    Q_OBJECT

public:
    GraphicsView(QWidget* parent = nullptr);
    ~GraphicsView();
    bool setupWidget();
    void clearImageInfo();
    void setImage(const QImage& image);

protected:
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    void fitFrame();
    void paintBackgroundBoard();
public slots:
    void onZoom(double scaleFactor);
    void onCenter();

private:
    double zoomValue = 1;
    QGraphicsScene* GVScene;
    ImageItem* GVImageItem;
    QWidget* ImageInfoWidget;
    QLabel* ImageInfoLabel;
    QImage GVImage;
    QPixmap backGroundPixmap = QPixmap(36,36);
};


#endif
