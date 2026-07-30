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
    void onCameraTcpActionToggled(bool enabled);

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
    void onCameraRunCreated(const InspectionRunRecord& run);
    void onCameraInferenceFinished(const DecodedCameraFrame& frame, const InferenceResult& result);
    void onCameraInferenceFailed(
        const DecodedCameraFrame& frame,
        const QString& errorMessage);
    void onCameraSuccessPersisted(
        const PersistedInspectionResult& result,
        quint64 transportSequence);
    void onCameraFailurePersisted(
        const InspectionItemPageRow& item,
        quint64 transportSequence);

private:
    void setupUI();
    void setupToolBar();
    void initializeCameraFrameSource();
    void initializePersistence();
    bool isCameraFrameProcessingAllowed() const;
    void initializeCameraInferenceController();
    void setCameraTcpActionState(bool enabled);
    bool sendCameraInferenceError(
        quint64 sequence,
        const QString& errorCode,
        const QString& message);
    void beginCameraPersistenceSession();
    void requestFinishCameraPersistenceSession();
    void tryFinalizeCameraPersistenceSession();
    void resetCameraPersistenceSession();
    void finalizeCameraPersistenceSessionBlocking();
    [[nodiscard]]
    CameraPersistenceFrame cameraPersistenceFrame(
        const DecodedCameraFrame& frame) const;

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
    bool cameraTcpEnabled = false;
    bool cameraClientConnected = false;
    QString cameraPeerAddress;
    quint16 cameraPeerPort = 0;
    QString cameraRunPeerAddress;
    quint16 cameraRunPeerPort = 0;
    QString cameraRunId;
    bool cameraRunReady = false;
    bool cameraSessionClosing = false;
    bool cameraFinalizeInFlight = false;
    qint64 cameraReceivedCount = 0;
    qint64 cameraDroppedCount = 0;
    qint64 cameraPersistenceInFlight = 0;
    bool closing = false;
    bool closeAfterSave = false;

    DetectionStatus detectionStatus = DetectionStatus::IDLE;
};

#endif
