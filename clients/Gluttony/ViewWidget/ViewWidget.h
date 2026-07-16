#ifndef VIEWWIDGET_H
#define VIEWWIDGET_H

#include "ViewWidgetUI.h"


class ViewWidget : public QWidget
{
    Q_OBJECT
public:
    ViewWidget(QWidget* parent = nullptr);
    ~ViewWidget();
    void setupUI();
public:
    ViewWidgetUI* ui;
};







#endif
