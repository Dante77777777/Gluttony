#include "LogWidget.h"


LogWidget::LogWidget()
    : QWidget()
    , ui(new LogWidgetUI())
{
    setAttribute(Qt::WA_StyledBackground, true);
    setup();
}


LogWidget::~LogWidget()
{

}


void LogWidget::setup()
{
    ui->setupUI(this);
    connect(&LogMessage::getInstance(),&LogMessage::sigDebugHtmlData,ui->logs,&QTextBrowser::append);
    connect(ui->clearButton,&QPushButton::clicked,this,&LogWidget::onLogClearButtonClicked);
}


void LogWidget::onLogClearButtonClicked()
{
    QDEBUG("LogView Clearbutton is Clicked");
    ui->logs->clear();
    QDEBUG("LogView is Cleared");
}
