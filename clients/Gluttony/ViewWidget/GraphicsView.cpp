#include "GraphicsView.h"

#define ZOOMMAX 50
#define ZOOMMIN 0.1

GraphicsView::GraphicsView(QWidget* parent)
    : QGraphicsView(parent)
    , GVScene(new QGraphicsScene(this))
    , GVImageItem(new ImageItem(this))
    , ImageInfoLabel(new QLabel(this))
    , ImageInfoWidget(new QWidget(this))
{
    this->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    this->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    this->setRenderHints(QPainter::Antialiasing);
    this->setRenderHints(QPainter::SmoothPixmapTransform);

    this->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);

    this->setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    this->setDragMode(QGraphicsView::ScrollHandDrag);

    this->setSceneRect(INT_MIN/2,INT_MIN/2,INT_MAX,INT_MAX);
    paintBackgroundBoard();
    centerOn(0,0);

    if(!setupWidget())
    {
        throw::std::bad_alloc();
    }
}


GraphicsView::~GraphicsView()
{
    GVScene->deleteLater();
    delete GVImageItem;
}


bool GraphicsView::setupWidget()
{
    this->setScene(GVScene);
    GVScene->addItem(GVImageItem);
    ImageInfoLabel->setStyleSheet("color:rgb(200,255,200); "
                                  "background-color:rgba(50,50,50,160); "
                                  "font: Microsoft YaHei;"
                                  "font-size: 15px;");
    ImageInfoLabel->setText(QString("W:0,H:0 | X:0,Y:0 | R:0,G:0,B:0"));

    ImageInfoWidget->setFixedHeight(25);
    ImageInfoWidget->setGeometry(0,this->height()-25,this->width(),25);
    ImageInfoWidget->setStyleSheet("background-color:rgba(0,0,0,0)");

    QHBoxLayout* imageInfoLayout = new QHBoxLayout(ImageInfoWidget);
    imageInfoLayout->setContentsMargins(0,0,0,0);
    imageInfoLayout->setSpacing(0);
    imageInfoLayout->addWidget(ImageInfoLabel);

    connect(GVImageItem,&ImageItem::ImageInfo,this,[this](QString imageInfo){
        ImageInfoLabel->setText(imageInfo);
    });

    return true;
}


void GraphicsView::clearImageInfo()
{
    ImageInfoLabel->setText(QString("W:0,H:0 | X:0,Y:0 | R:0,G:0,B:0"));
    setImage(QImage());
}


void GraphicsView::setImage(const QImage& image)
{
    static QMutex mutex;
    QMutexLocker<QMutex> lock(&mutex);

    GVImage = image.copy();
    auto GVpixmap = QPixmap::fromImage(image);
    GVImageItem->setPixmap(GVpixmap);
    GVImageItem->setW(GVpixmap.width());
    GVImageItem->setH(GVpixmap.height());
    fitFrame();
    onCenter();
    show();
}


void GraphicsView::mouseDoubleClickEvent(QMouseEvent* event)
{
    if(event->button() == Qt::LeftButton)
    {
        fitFrame();
        onCenter();
    }
    QGraphicsView::mouseDoubleClickEvent(event);
}


void GraphicsView::wheelEvent(QWheelEvent* event)
{
    QPoint scrollAmount = event->angleDelta();
    if((scrollAmount.y() > 0) && (zoomValue >= ZOOMMAX))
    {
        return ;
    }
    if((scrollAmount.y() < 0) && (zoomValue <= ZOOMMIN))
    {
        return ;
    }
    scrollAmount.y() > 0 ? onZoom(1.1) : onZoom(0.9);
    event->accept();
}


void GraphicsView::resizeEvent(QResizeEvent* event)
{
    fitFrame();
    onCenter();
    ImageInfoWidget->setGeometry(0,this->height()-25,this->width(),25);
    QGraphicsView::resizeEvent(event);
}


void GraphicsView::paintEvent(QPaintEvent* event)
{
    QPainter painter(this->viewport());
    painter.drawTiledPixmap(QRect(QPoint(0,0),QPoint(this->width(),this->height())),backGroundPixmap);
    QGraphicsView::paintEvent(event);
}


void GraphicsView::fitFrame()
{
    if(this->width() < 1 || GVImage.height() < 1)
    {
        return ;
    }
    double viewWidth = this->width();
    double viewHeight = this->height();
    double widthScale = (GVImage.width()+1) / viewWidth;
    double heightScale = (GVImage.height()+1) / viewHeight;

    double minScale = (widthScale >= heightScale) ? 1/widthScale : 1/heightScale;
    double finalScale = minScale / zoomValue;

    onZoom(finalScale);
    zoomValue = minScale;
}


void GraphicsView::paintBackgroundBoard()
{
    backGroundPixmap.fill(QColor("#111827"));
}


void GraphicsView::onZoom(double scaleFactor)
{
    if (GVImageItem->pixmap().isNull()) {
        return;
    }
    zoomValue *= scaleFactor;
    this->scale(scaleFactor,scaleFactor);
}


void GraphicsView::onCenter()
{
    if (GVImageItem->pixmap().isNull()) {
        return;
    }
    this->centerOn(GVImageItem->pixmap().width()/2,GVImageItem->pixmap().height()/2);
    GVImageItem->setPos(0,0);
}


void GraphicsView::onFit()
{
    if (GVImageItem->pixmap().isNull()) {
        return;
    }
    resetTransform();
    fitInView(GVImageItem,Qt::KeepAspectRatio);
    zoomValue = transform().m11();
    centerOn(GVImageItem);
}


void GraphicsView::onRotateLeft()
{
    if (GVImageItem->pixmap().isNull()) {
        return;
    }
    GVImageItem->setTransformOriginPoint(GVImageItem->boundingRect().center());
    GVImageItem->setRotation(GVImageItem->rotation()-90);
    onFit();
}


void GraphicsView::onRotateRight()
{
    if (GVImageItem->pixmap().isNull()) {
        return;
    }
    GVImageItem->setTransformOriginPoint(GVImageItem->boundingRect().center());
    GVImageItem->setRotation(GVImageItem->rotation()+90);
    onFit();
}
