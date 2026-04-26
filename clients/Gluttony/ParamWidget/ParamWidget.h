#ifndef PARAMWIDGET_H
#define PARAMWIDGET_H
#include <QWidget>

#include "ParamWidgetUI.h"
class ParamWidget : public QWidget
{
    Q_OBJECT;
public:
    ParamWidget(QWidget* parent = nullptr);
    ~ParamWidget();
    void setupUI();
private:
    ParamWidgetUI* ui;
};















#endif
