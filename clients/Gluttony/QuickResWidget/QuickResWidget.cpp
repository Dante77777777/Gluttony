#include "QuickResWidget.h"

QuickResWidget::QuickResWidget(QWidget* parent)
    : QWidget()
    , ui(new QuickResWidgetUI())
{
    setAttribute(Qt::WA_StyledBackground, true);
    setup();
}

QuickResWidget::~QuickResWidget()
{

}


void QuickResWidget::setup()
{
    ui->setupUI(this);
}
