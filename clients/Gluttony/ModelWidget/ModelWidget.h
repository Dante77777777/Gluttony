#ifndef MODELWIDGET_H
#define MODELWIDGET_H
#include <QWidget>
#include <QObject>

#include "ModelWidgetUI.h"

class ModelWidget : public QWidget
{
    Q_OBJECT;
public:
    ModelWidget(QWidget* parent = nullptr);
    ~ModelWidget();
    void setupUI();
private:
    ModelWidgetUI* ui;
};

























#endif
