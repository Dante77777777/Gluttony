#include "ViewWidget.h"
#include <QFileInfo>


ViewWidget::ViewWidget(QWidget* parent)
    :ui(new ViewWidgetUI)
{
    setupUI();
}


ViewWidget::~ViewWidget()
{

}


void ViewWidget::setupUI()
{
    ui->setupUI(this);

    connect(ui->originalGV, &GraphicsView::zoomChanged, this,
            [this](double zoomScale) {
                originalZoomScale = zoomScale;
                updateOriginalFooter();
            });
}

void ViewWidget::setOriginalImagePath(const QString& imagePath)
{
    originalFileName = QFileInfo(imagePath).fileName();
    updateOriginalFooter();
}

void ViewWidget::updateOriginalFooter()
{
    if (originalFileName.isEmpty()) {
        ui->originalBLabel->setText("No image selected");
        return;
    }

    const QString zoomText = originalZoomScale > 0.0
        ? QString("%1%").arg(qRound(originalZoomScale * 100.0))
        : "--";

    ui->originalBLabel->setText(
        QString("%1  |  Zoom: %2").arg(originalFileName, zoomText));
}
