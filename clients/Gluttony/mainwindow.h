#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QThread>

#include "DataBase/PersistenceRecords.h"
#include "DataBase/PersistenceWorker.h"
#include "DetectionResult/DetectionResult.h"
#include "Inference/InferenceEngine.h"
#include "LogWidget/LogWidget.h"
#include "ModelWidget/ModelWidget.h"
#include "ParamWidget/ParamWidget.h"
#include "QuickResWidget/QuickResWidget.h"
#include "ToolBar/ToolBar.h"
#include "ViewWidget/ViewWidget.h"
#include "Workers/ImageWorker.h"
#include "statusBar/StatusBar.h"
#include "Transport/CameraFrameSource.h"
#include "Workers/CameraInferenceController.h"

class QCloseEvent;

enum class InputMode
{
    NONE,
    SINGLEIMAGE,
    FOLDER
};

enum class DetectionStatus
{
    IDLE,
    PREPARING,
    RUNNING,
    PAUSEREQUEST,
    PAUSED,
    FINALIZING
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(
        QString dataBaseFilePath,
        QString managedRunsRoot,
        QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onModelSelected(const QString& modelPath);
    void onImageSelected(const QImage& image,const QString& imagePath);
    void onFolderSelected(const QString& folderPath);
    void onStartActionTriggered();
    void onStopActionTriggered();
    void onSaveBatchActionTriggered();
    void onDiscardBatchActionTriggered();
    void onBatchManagerActionTriggered();
    void onRecordClicked(const DetectionRecord& record);

    void onPersistenceInitialized(bool success,const QString& errorMessage);
    void onInterruptedRunsReady(const InspectionRunRecords& runs);
    void onRunCreated(const InspectionRunRecord& run);
    void onItemClaimed(const ClaimedInspectionItem& item);
    void onPersistenceOperationFailed(const QString& operation,const QString& contextId,const QString& errorMessage);

    void onInferenceReady();
    void onInferenceInitializationFailed(const QString& errorMessage);
    void onTaskFinished(const ClaimedInspectionItem& item,const InferenceResult& result);
    void onTaskFailed(const ClaimedInspectionItem& item,const QString& errorMessage);

    void onSuccessPersisted(const PersistedInspectionResult& result);
    void onFailurePersisted(const InspectionItemPageRow& item);
    void onRunDrafted(const InspectionRunRecord& run);
    void onRunSaved(const QString& runId);
    void onRunDiscarded(const QString& runId);
    void onBatchCatalogReady(const InspectionRunRecords& runs);
    void onRunLoaded(const InspectionRunRecord& run,const InspectionItemRows& items,bool resumed);
    void onSavedRunDeleted(const QString& runId);
    void onRunInterrupted(const QString& runId);
    void onCameraFrameReady(const DecodedCameraFrame& frame);

private:
    void setupUI();
    void setupToolBar();
    void initializeCameraFrameSource();
    void initializePersistence();
    void initializeCameraInferenceController();

    void beginNewRun(const QString& sourceType, const QString& sourcePath);
    void startInferenceWorker(const QString& modelPath,const InferenceOptions& options);
    void stopInferenceWorker();
    void requestNextItem();
    void continueAfterPersistedItem();
    void finishActiveRun();
    void enterPausedState();

    void resetStatusBar();
    void refreshStatusBar(const QString& stateText);
    void updateActionStates();
    void setInputSelectionEnabled(bool enabled);
    void clearActiveRun();

    void populateRunResults(
        const InspectionRunRecord& run,
        const InspectionItemRows& items);
    void displayRecord(const DetectionRecord& record);
    [[nodiscard]]
    DetectionRecord recordFromPersisted(
        const PersistedInspectionResult& result) const;
    [[nodiscard]]
    DetectionRecord recordFromStoredItem(
        const InspectionRunRecord& run,
        const InspectionItemPageRow& item) const;

    void promptToSaveDraft();
    void requestSaveDraft();
    void requestDiscardDraft();
    void requestBatchCatalog();
    void requestResumeRun(const QString& runId);
    void requestLoadSavedRun(const QString& runId);
    void requestDeleteSavedRun(const QString& runId);
    void interruptActiveRunBlocking();
    void discardActiveDraftBlocking();

    QWidget* centralWidget = nullptr;
    ToolBar* topToolBar = nullptr;
    StatusBar* statusBar = nullptr;
    ViewWidget* viewWidget = nullptr;
    CameraFrameSource* cameraFrameSource = nullptr;
    CameraInferenceController* cameraInferenceController = nullptr;
    InferenceEngine inferenceEngine;
    ModelWidget* modelWidget = nullptr;
    ParamWidget* paramWidget = nullptr;
    DetectionResult* DRWidget = nullptr;
    QuickResWidget* quickResWidget = nullptr;
    LogWidget* logWidget = nullptr;
    QHBoxLayout* mainLayout = nullptr;
    QVBoxLayout* leftLayout = nullptr;
    QVBoxLayout* rightLayout = nullptr;
    QVBoxLayout* dataLayout = nullptr;
    QHBoxLayout* viewLayout = nullptr;
    QHBoxLayout* leftTopLayout = nullptr;

    QString selectedImagePath;
    QString selectedFolderPath;
    QString loadedModelPath;
    InputMode inputMode = InputMode::NONE;

    QString dataBaseFilePath;
    QString managedRunsRoot;
    QThread* persistenceThread = nullptr;
    PersistenceWorker* persistenceWorker = nullptr;
    bool persistenceReady = false;

    QThread* inferenceThread = nullptr;
    ImageWorker* imageWorker = nullptr;
    bool inferenceReady = false;

    QString activeRunId;
    QString activeRunStatus;
    QString activeStoragePath;
    InferenceOptions activeInferenceOptions;

    qint64 totalImageCount = 0;
    qint64 completedImageCount = 0;
    qint64 failedImageCount = 0;
    qint64 ngImageCount = 0;
    qint64 okImageCount = 0;
    qint64 reviewImageCount = 0;
    double totalScore = 0.0;

    bool claimInFlight = false;
    bool itemInFlight = false;
    bool persistenceInFlight = false;
    bool finishInFlight = false;
    bool closing = false;
    bool closeAfterSave = false;

    DetectionStatus detectionStatus = DetectionStatus::IDLE;
};

#endif
