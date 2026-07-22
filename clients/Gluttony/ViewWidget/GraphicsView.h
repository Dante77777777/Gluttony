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

signals:
    // 当前视图缩放比例。1.0 表示以图像原始像素大小显示。
    void zoomChanged(double zoomScale);

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
    void onFit();
    void onRotateLeft();
    void onRotateRight();

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
