#ifndef LOGWIDGET_H
#define LOGWIDGET_H

#include "LogWidgetUI.h"
#include "LogMessage.h"

class LogWidget : public QWidget
{
    Q_OBJECT
public:
    LogWidget();
    ~LogWidget();
    void setup();
public slots:
    void onLogClearButtonClicked();
public:
    LogWidgetUI* ui;
};







#endif
