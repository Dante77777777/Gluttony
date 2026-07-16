#ifndef MODELWIDGET_H
#define MODELWIDGET_H
#include <QWidget>
#include <QObject>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QMessageBox>

#include "ModelWidgetUI.h"
#include "../LogWidget/LogMessage.h"

class ModelWidget : public QWidget
{
    Q_OBJECT;
public:
    ModelWidget(QWidget* parent = nullptr);
    ~ModelWidget();
    void setupUI();

signals:
    void sigModelSelect(const QString& xmlPath);
private slots:
    void onSelectModelButtonClicked();
private:
    ModelWidgetUI* ui;
};

























#endif
