#ifndef PARAMWIDGET_H
#define PARAMWIDGET_H
#include <QWidget>
#include <QDir>
#include <QFileDialog>
#include <QMessageBox>
#include <QImage>

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
signals:
    void sigImageSlected(const QImage& iamge, const QString& imagePath);

public slots:
    void onSelectImageButtonClicked();
    void onSelectFolderButtonClicked();
private:
    ParamWidgetUI* ui;
    ViewWidget* viewWidget;
};















#endif
