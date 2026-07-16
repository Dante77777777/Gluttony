#include "ViewWidget.h"


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
}
