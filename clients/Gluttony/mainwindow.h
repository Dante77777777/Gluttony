#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QWidget>
#include <QPushButton>
#include <QBoxLayout>
#include <QFile>
#include <QImage>

#include "ToolBar/ToolBar.h"
#include "ModelWidget/ModelWidget.h"
#include "ParamWidget/ParamWidget.h"
#include "ViewWidget/ViewWidget.h"
#include "DetectionResult/DetectionResult.h"
#include "statusBar/StatusBar.h"
#include "QuickResWidget/QuickResWidget.h"
#include "LogWidget/LogWidget.h"
#include "Inference/InferenceEngine.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    void setupUI();
    void setupToolBar();
private slots:
    void onModelSelected(const QString& xmlPath);
    void onImageSelected(const QImage image, const QString imagePath);
    void onStartActionTriggered();

private:
    QWidget* centralWidget;
    ToolBar* topToolBar;
    StatusBar* statusBar;
    ViewWidget* viewWidget;
    InferenceEngine inferenceEngine;
    ModelWidget* modelWidget;
    ParamWidget* paramWidget;
    DetectionResult* DRWidget;
    QuickResWidget* quickResWidget;
    LogWidget* logWidget;
    QHBoxLayout* mainLayout;
    QVBoxLayout* leftLayout;
    QVBoxLayout* rightLayout;
    QVBoxLayout* dataLayout;
    QHBoxLayout* viewLayout;
    QHBoxLayout* leftTopLayout;

    QImage selectedImage;
    QString slectedImagePath;

};
#endif // MAINWINDOW_H
