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
    void sigImageSelected(const QImage& iamge, const QString& imagePath);
    void sigFolderSelected(const QString& folderPath);
public slots:
    void onSelectImageButtonClicked();
    void onSelectFolderButtonClicked();
public:
    ParamWidgetUI* ui;
private:
    ViewWidget* viewWidget;
};















#endif
