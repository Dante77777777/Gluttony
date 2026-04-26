#include "ModelWidget.h"

ModelWidget::ModelWidget(QWidget* parent)
    : QWidget(parent)
    , ui(new ModelWidgetUI())
{
    this->setObjectName("ModelWidget");
    setAttribute(Qt::WA_StyledBackground, true);
    setupUI();
}

ModelWidget::~ModelWidget()
{

}

void ModelWidget::setupUI()
{
    ui->setupUI(this);
    this->setFixedWidth(300);
    this->setFixedHeight(130);
}
