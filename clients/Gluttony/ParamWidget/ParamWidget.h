#ifndef PARAMWIDGET_H
#define PARAMWIDGET_H
#include <QWidget>
#include <QDir>
#include <QFileDialog>
#include <QMessageBox>

#include "ParamWidgetUI.h"
#include "../LogWidget/LogMessage.h"
#include "../ViewWidget/ViewWidget.h"

class ParamWidget : public QWidget
{
    Q_OBJECT;
public:
    ParamWidget(ViewWidget* viewWidget_, QWidget* parent = nullptr);
    ~ParamWidget();
    void setupUI();
public slots:
    void onSelectImageButtonClicked();
    void onSelectFolderButtonClicked();
private:
    ParamWidgetUI* ui;
    ViewWidget* viewWidget;
};















#endif
