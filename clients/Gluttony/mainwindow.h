#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QWidget>
#include <QPushButton>
#include <QBoxLayout>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QThread>
#include <memory>
#include <QDir>

#include "ToolBar/ToolBar.h"
#include "ModelWidget/ModelWidget.h"
#include "ParamWidget/ParamWidget.h"
#include "ViewWidget/ViewWidget.h"
#include "DetectionResult/DetectionResult.h"
#include "statusBar/StatusBar.h"
#include "QuickResWidget/QuickResWidget.h"
#include "LogWidget/LogWidget.h"
#include "Inference/InferenceEngine.h"
#include "Workers/ImageProducer.h"
#include "Workers/ImageWorker.h"

enum class InputMode
{
    NONE,
    SINGLEIMAGE,
    FOLDER
};

enum class DetectionStatus
{
    IDLE,
    RUNNING,
    PAUSEREQUEST,
    PAUSED
};

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
    void onFolderSelected(const QString& folderPath);
    void onTaskFinished(const QString& filePath, const QImage& sourceImage, const InferenceResult& result);
    void onTaskFailed(const QString& filePath, const QString& errorMessage);
    void onBatchFinished();
    void onStopActionTriggered();
    void onDetectionPaused();
    void onRecordClicked(const DetectionRecord& record);

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

private:
    InputMode inputMode = InputMode::NONE;

    QString selectedFolderPath;
    QString loadedModelPath;

    std::shared_ptr<ImageQueue> imageQueue;

    QThread* producerThread = nullptr;
    QThread* inferenceThread = nullptr;

    ImageProducer* imageProducer = nullptr;
    ImageWorker* imageWorker = nullptr;

    int totalImageCount = 0;
    int completedImageCount = 0;
    int ngImageCount = 0;
    int okImageCount = 0;
    double totalScore = 0;

    void startDetection(const QStringList& singleFiles);
    void startFolderDetection(const QString& folderPath);
    void startWorkers();
    void resetStatusBar();
    void refreshStatusBar(const QString& stateText);

private:
    DetectionStatus detectionStatus = DetectionStatus::IDLE;
};
#endif // MAINWINDOW_H
