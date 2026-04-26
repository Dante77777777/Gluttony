#include "ParamWidget.h"

ParamWidget::ParamWidget(QWidget* parent)
    : QWidget(parent)
    , ui(new ParamWidgetUI())
{
    this->setObjectName("ParamWidget");
    setAttribute(Qt::WA_StyledBackground, true);
    setupUI();
}

ParamWidget::~ParamWidget()
{

}

void ParamWidget::setupUI()
{
    ui->setupUI(this);
    this->setFixedWidth(300);
    this->setFixedHeight(380);
}
