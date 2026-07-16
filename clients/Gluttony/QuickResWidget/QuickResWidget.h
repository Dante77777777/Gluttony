#ifndef QUICKRESWIDGET_H
#define QUICKRESWIDGET_H

#include "QuickResWidgetUI.h"

class QuickResWidget : public QWidget
{
    Q_OBJECT
public:
    QuickResWidget(QWidget* parent = nullptr);
    ~QuickResWidget();
    void setup();
public:
    QuickResWidgetUI* ui;
};











#endif
