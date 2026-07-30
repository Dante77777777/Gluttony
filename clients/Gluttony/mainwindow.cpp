#include "mainwindow.h"

#include <QCloseEvent>
#include <QCoreApplication>
#include <QDateTime>
#include <QDialog>
#include <QDir>
#include <QFileInfo>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QMessageBox>
#include <QMetaObject>
#include <QPushButton>
#include <QTableWidget>
#include <QTimer>
#include <QUuid>
#include <QVBoxLayout>

namespace MainWindowUtils
{

constexpr quint16 CAMERA_STREAM_PORT = 9000;

DetectionLevel detectionLevelFromText(const QString& value)
{
    if (value == QStringLiteral("NG"))
    {
        return DetectionLevel::NG;
    }
    if (value == QStringLiteral("REVIEW"))
    {
        return DetectionLevel::REVIEW;
    }
    return DetectionLevel::OK;
}

QString displayFileName(const QString& path)
{
    const QFileInfo info(path);
    return QStringLiteral("%1/%2")
        .arg(info.dir().dirName(), info.fileName());
}

}

MainWindow::MainWindow(
    QString dataBaseFilePathValue,
    QString managedRunsRootValue,
    QWidget* parent)
    : QMainWindow(parent)
    , centralWidget(new QWidget(this))
    , topToolBar(new ToolBar())
    , statusBar(new StatusBar())
    , viewWidget(new ViewWidget(this))
    , modelWidget(new ModelWidget(this))
    , paramWidget(new ParamWidget(viewWidget, this))
    , DRWidget(new DetectionResult(this))
    , quickResWidget(new QuickResWidget(this))
    , logWidget(new LogWidget())
    , mainLayout(new QHBoxLayout())
    , leftLayout(new QVBoxLayout())
    , rightLayout(new QVBoxLayout())
    , dataLayout(new QVBoxLayout())
    , viewLayout(new QHBoxLayout())
    , leftTopLayout(new QHBoxLayout())
    , dataBaseFilePath(QDir::cleanPath(std::move(dataBaseFilePathValue)))
    , managedRunsRoot(QDir::cleanPath(std::move(managedRunsRootValue)))
{
    statusBar->ui->setupUI(this);
    setupToolBar();
    setupUI();
    initializeCameraInferenceController();
    initializeCameraFrameSource();
    initializePersistence();
    updateActionStates();
}

MainWindow::~MainWindow()
{
    closing = true;

    if(cameraFrameSource != nullptr)
    {
        cameraFrameSource->stop();
    }

    if(cameraInferenceController != nullptr)
    {
        cameraInferenceController->stop();
    }

    stopInferenceWorker();

    if (persistenceThread != nullptr && persistenceThread->isRunning())
    {
        if (persistenceWorker != nullptr)
        {
            QMetaObject::invokeMethod(
                persistenceWorker,
                [worker = persistenceWorker]()
                {
                    worker->shutdown();
                },
                Qt::BlockingQueuedConnection);
        }
        persistenceThread->quit();
        persistenceThread->wait();
    }
}

void MainWindow::setupToolBar()
{
    topToolBar->ui->setupUI(this);
    connect(
        topToolBar->ui->openImageAction,
        &QAction::triggered,
        paramWidget,
        &ParamWidget::onSelectImageButtonClicked);
    connect(
        topToolBar->ui->openFolderAction,
        &QAction::triggered,
        paramWidget,
        &ParamWidget::onSelectFolderButtonClicked);
    connect(
        topToolBar->ui->startAction,
        &QAction::triggered,
        this,
        &MainWindow::onStartActionTriggered);
    connect(
        topToolBar->ui->stopAction,
        &QAction::triggered,
        this,
        &MainWindow::onStopActionTriggered);
    connect(
        topToolBar->ui->saveBatchAction,
        &QAction::triggered,
        this,
        &MainWindow::onSaveBatchActionTriggered);
    connect(
        topToolBar->ui->discardBatchAction,
        &QAction::triggered,
        this,
        &MainWindow::onDiscardBatchActionTriggered);
    connect(
        topToolBar->ui->batchManagerAction,
        &QAction::triggered,
        this,
        &MainWindow::onBatchManagerActionTriggered);

    connect(
        topToolBar->ui->zoomOutAction,
        &QAction::triggered,
        viewWidget->ui->originalWidget,
        [this]()
        {
            viewWidget->ui->originalGV->onZoom(0.9);
        });
    connect(
        topToolBar->ui->zoomOutAction,
        &QAction::triggered,
        viewWidget->ui->resultWidget,
        [this]()
        {
            viewWidget->ui->resultGV->onZoom(0.9);
        });
    connect(
        topToolBar->ui->zoomInAction,
        &QAction::triggered,
        viewWidget->ui->originalWidget,
        [this]()
        {
            viewWidget->ui->originalGV->onZoom(1.1);
        });
    connect(
        topToolBar->ui->zoomInAction,
        &QAction::triggered,
        viewWidget->ui->resultWidget,
        [this]()
        {
            viewWidget->ui->resultGV->onZoom(1.1);
        });
    connect(
        topToolBar->ui->fitAction,
        &QAction::triggered,
        viewWidget->ui->originalGV,
        &GraphicsView::onFit);
    connect(
        topToolBar->ui->fitAction,
        &QAction::triggered,
        viewWidget->ui->resultGV,
        &GraphicsView::onFit);
    connect(
        topToolBar->ui->rotateLeftAction,
        &QAction::triggered,
        viewWidget->ui->originalGV,
        &GraphicsView::onRotateLeft);
    connect(
        topToolBar->ui->rotateLeftAction,
        &QAction::triggered,
        viewWidget->ui->resultGV,
        &GraphicsView::onRotateLeft);
    connect(
        topToolBar->ui->rotateRightAction,
        &QAction::triggered,
        viewWidget->ui->originalGV,
        &GraphicsView::onRotateRight);
    connect(
        topToolBar->ui->rotateRightAction,
        &QAction::triggered,
        viewWidget->ui->resultGV,
        &GraphicsView::onRotateRight);

    connect(
        modelWidget,
        &ModelWidget::sigModelSelect,
        this,
        &MainWindow::onModelSelected);
    connect(
        paramWidget,
        &ParamWidget::sigImageSelected,
        this,
        &MainWindow::onImageSelected);
    connect(
        paramWidget,
        &ParamWidget::sigFolderSelected,
        this,
        &MainWindow::onFolderSelected);
}

void MainWindow::setupUI()
{
    resize(1700, 950);
    setWindowTitle(QStringLiteral("Gluttony Vision"));
    setWindowIcon(QIcon(QStringLiteral(":/Detective.png")));
    addToolBar(Qt::TopToolBarArea, topToolBar->ui->topToolBar);
    setStatusBar(statusBar->ui->statusBar);

    centralWidget->setLayout(mainLayout);
    mainLayout->addLayout(leftLayout, 5);
    mainLayout->addLayout(rightLayout, 1);
    leftLayout->addLayout(leftTopLayout, 5);
    leftLayout->addWidget(DRWidget, 2);
    leftTopLayout->addLayout(dataLayout);
    leftTopLayout->addLayout(viewLayout);
    leftTopLayout->setAlignment(dataLayout, Qt::AlignTop);
    rightLayout->addWidget(quickResWidget, 1);
    rightLayout->addWidget(logWidget, 1);
    dataLayout->addWidget(modelWidget);
    dataLayout->addWidget(paramWidget);
    viewLayout->addWidget(viewWidget);
    setCentralWidget(centralWidget);

    connect(
        DRWidget,
        &DetectionResult::sigRecord,
        this,
        &MainWindow::onRecordClicked);
}


void MainWindow::initializeCameraInferenceController()
{
    cameraInferenceController = new CameraInferenceController(this);

    connect(cameraInferenceController, &CameraInferenceController::sigReady,this,[this](){
        statusBar->ui->readyLabel->setText(QStringLiteral("Camera inference ready"));

        QDEBUG(QStringLiteral("Camera inference controller ready"));
    });

    connect(cameraInferenceController,&CameraInferenceController::sigInitializationFailed,this,[this](const QString& errorMessage){
        statusBar->ui->readyLabel->setText(QStringLiteral("Camera inference initialization failed"));
        QWARNING(QStringLiteral("Camera inference initialization failed: %1").arg(errorMessage));
    });

    connect(cameraInferenceController,&CameraInferenceController::sigStopped,this,[](){
        QDEBUG(QStringLiteral("Camera inference controller stopped"));
    });
}


void MainWindow::initializeCameraFrameSource()
{
    cameraFrameSource =
        new CameraFrameSource(this);

    connect(
        cameraFrameSource,
        &CameraFrameSource::sigServerStarted,
        this,
        [this](quint16 port)
        {
            statusBar->ui->readyLabel->setText(
                QStringLiteral(
                    "Camera server listening on port %1")
                    .arg(port));

            QDEBUG(
                QStringLiteral(
                    "Camera server started on port %1")
                    .arg(port));
        });

    connect(
        cameraFrameSource,
        &CameraFrameSource::sigServerStopped,
        this,
        [this]()
        {
            if(!closing)
            {
                statusBar->ui->readyLabel->setText(
                    QStringLiteral(
                        "Camera server stopped"));
            }

            QDEBUG(
                QStringLiteral(
                    "Camera server stopped"));
        });

    connect(
        cameraFrameSource,
        &CameraFrameSource::sigClientConnected,
        this,
        [this](
            const QString& peerAddress,
            quint16 peerPort)
        {
            statusBar->ui->readyLabel->setText(
                QStringLiteral(
                    "Camera connected: %1:%2")
                    .arg(peerAddress)
                    .arg(peerPort));

            QDEBUG(
                QStringLiteral(
                    "Camera client connected: %1:%2")
                    .arg(peerAddress)
                    .arg(peerPort));
        });

    connect(
        cameraFrameSource,
        &CameraFrameSource::sigClientDisconnected,
        this,
        [this]()
        {
            statusBar->ui->readyLabel->setText(
                QStringLiteral(
                    "Waiting for camera on port %1")
                    .arg(
                        MainWindowUtils::
                        CAMERA_STREAM_PORT));

            QWARNING(
                QStringLiteral(
                    "Camera client disconnected"));
        });

    connect(
        cameraFrameSource,
        &CameraFrameSource::sigClientRejected,
        this,
        [](const QString& reason)
        {
            QWARNING(
                QStringLiteral(
                    "Camera client rejected: %1")
                    .arg(reason));
        });

    connect(
        cameraFrameSource,
        &CameraFrameSource::sigFrameReady,
        this,
        &MainWindow::onCameraFrameReady);

    connect(
        cameraFrameSource,
        &CameraFrameSource::sigTransportError,
        this,
        [](const QString& errorMessage)
        {
            QWARNING(
                QStringLiteral(
                    "Camera transport error: %1")
                    .arg(errorMessage));
        });

    connect(
        cameraFrameSource,
        &CameraFrameSource::sigProtocolError,
        this,
        [](const QString& errorMessage)
        {
            QWARNING(
                QStringLiteral(
                    "Camera protocol error: %1")
                    .arg(errorMessage));
        });

    connect(
        cameraFrameSource,
        &CameraFrameSource::sigImageDecodeError,
        this,
        [](
            quint64 sequence,
            const QString& errorMessage)
        {
            QWARNING(
                QStringLiteral(
                    "Camera frame %1 decode error: %2")
                    .arg(sequence)
                    .arg(errorMessage));
        });

    connect(
        cameraFrameSource,
        &CameraFrameSource::sigBufferOverflow,
        this,
        [](qsizetype bufferedBytes)
        {
            QWARNING(
                QStringLiteral(
                    "Camera receive buffer overflow: "
                    "%1 bytes")
                    .arg(bufferedBytes));
        });

    QString errorMessage;

    if(!cameraFrameSource->start(
            QHostAddress::AnyIPv4,
            MainWindowUtils::CAMERA_STREAM_PORT,
            &errorMessage))
    {
        statusBar->ui->readyLabel->setText(
            QStringLiteral(
                "Camera server failed to start"));

        QCRITICAL(
            QStringLiteral(
                "Failed to start camera server: %1")
                .arg(errorMessage));
    }
}


void MainWindow::initializePersistence()
{
    persistenceThread = new QThread(this);
    persistenceWorker = new PersistenceWorker(
        dataBaseFilePath,
        managedRunsRoot,
        QStringLiteral("gluttony.persistence"),
        nullptr);
    persistenceWorker->moveToThread(persistenceThread);

    connect(
        persistenceThread,
        &QThread::started,
        persistenceWorker,
        &PersistenceWorker::initialize);
    connect(
        persistenceThread,
        &QThread::finished,
        persistenceWorker,
        &QObject::deleteLater);
    connect(
        persistenceWorker,
        &PersistenceWorker::initialized,
        this,
        &MainWindow::onPersistenceInitialized);
    connect(
        persistenceWorker,
        &PersistenceWorker::interruptedRunsReady,
        this,
        &MainWindow::onInterruptedRunsReady);
    connect(
        persistenceWorker,
        &PersistenceWorker::runCreated,
        this,
        &MainWindow::onRunCreated);
    connect(
        persistenceWorker,
        &PersistenceWorker::itemClaimed,
        this,
        &MainWindow::onItemClaimed);
    connect(
        persistenceWorker,
        &PersistenceWorker::successPersisted,
        this,
        &MainWindow::onSuccessPersisted);
    connect(
        persistenceWorker,
        &PersistenceWorker::failurePersisted,
        this,
        &MainWindow::onFailurePersisted);
    connect(
        persistenceWorker,
        &PersistenceWorker::runDrafted,
        this,
        &MainWindow::onRunDrafted);
    connect(
        persistenceWorker,
        &PersistenceWorker::runSaved,
        this,
        &MainWindow::onRunSaved);
    connect(
        persistenceWorker,
        &PersistenceWorker::runDiscarded,
        this,
        &MainWindow::onRunDiscarded);
    connect(
        persistenceWorker,
        &PersistenceWorker::batchCatalogReady,
        this,
        &MainWindow::onBatchCatalogReady);
    connect(
        persistenceWorker,
        &PersistenceWorker::runLoaded,
        this,
        &MainWindow::onRunLoaded);
    connect(
        persistenceWorker,
        &PersistenceWorker::savedRunDeleted,
        this,
        &MainWindow::onSavedRunDeleted);
    connect(
        persistenceWorker,
        &PersistenceWorker::runInterrupted,
        this,
        &MainWindow::onRunInterrupted);
    connect(
        persistenceWorker,
        &PersistenceWorker::operationFailed,
        this,
        &MainWindow::onPersistenceOperationFailed);

    persistenceThread->start();
}

void MainWindow::onPersistenceInitialized(
    bool success,
    const QString& errorMessage)
{
    persistenceReady = success;
    if (!success)
    {
        QCRITICAL(
            QStringLiteral("Persistence Worker Initialization Failed: %1")
                .arg(errorMessage));
        QMessageBox::critical(
            this,
            QStringLiteral("Database Error"),
            QStringLiteral(
                "The persistence worker could not be initialized.\n\n%1")
                .arg(errorMessage));
    }
    else
    {
        QDEBUG(QStringLiteral("Persistence Worker Initialized."));
    }
    updateActionStates();
}

void MainWindow::onModelSelected(const QString& modelPath)
{
    if (detectionStatus != DetectionStatus::IDLE
        || activeRunStatus == QStringLiteral("DRAFT"))
    {
        return;
    }

    QString errorMessage;
    if (!inferenceEngine.loadModel(modelPath, &errorMessage))
    {
        QWARNING(errorMessage);
        QMessageBox::critical(
            this,
            QStringLiteral("Model Loading Failed"),
            errorMessage);
        return;
    }

    loadedModelPath = QDir::cleanPath(QFileInfo(modelPath).absoluteFilePath());
    QDEBUG(
        QStringLiteral("Model loaded successfully: %1")
            .arg(loadedModelPath));
    for (const QString& input : inferenceEngine.inputDescriptions())
    {
        QDEBUG(input);
    }

    for (const QString& output : inferenceEngine.outputDescriptions())
    {
        QDEBUG(output);
    }

    if(cameraInferenceController != nullptr)
    {
        statusBar->ui->readyLabel->setText(QStringLiteral("Loading camera inference model..."));

        cameraInferenceController->start(loadedModelPath,paramWidget->inferenceOptions());
    }
    updateActionStates();
}

void MainWindow::onImageSelected(
    const QImage& image,
    const QString& imagePath)
{
    Q_UNUSED(image);
    selectedImagePath =
        QDir::cleanPath(QFileInfo(imagePath).absoluteFilePath());
    selectedFolderPath.clear();
    inputMode = InputMode::SINGLEIMAGE;
    updateActionStates();
}

void MainWindow::onFolderSelected(const QString& folderPath)
{
    selectedFolderPath =
        QDir::cleanPath(QFileInfo(folderPath).absoluteFilePath());
    selectedImagePath.clear();
    inputMode = InputMode::FOLDER;
    updateActionStates();
}

void MainWindow::onStartActionTriggered()
{
    if (detectionStatus == DetectionStatus::PAUSED)
    {
        detectionStatus = DetectionStatus::RUNNING;
        updateActionStates();
        requestNextItem();
        return;
    }

    if (detectionStatus != DetectionStatus::IDLE
        || !persistenceReady)
    {
        return;
    }
    if (!inferenceEngine.isLoaded() || loadedModelPath.isEmpty())
    {
        QMessageBox::warning(
            this,
            QStringLiteral("Model Not Loaded"),
            QStringLiteral(
                "Please load an OpenVINO IR or ONNX model first."));
        return;
    }

    if (inputMode == InputMode::SINGLEIMAGE
        && !selectedImagePath.isEmpty())
    {
        beginNewRun(QStringLiteral("SINGLE_IMAGE"), selectedImagePath);
        return;
    }
    if (inputMode == InputMode::FOLDER
        && !selectedFolderPath.isEmpty())
    {
        beginNewRun(QStringLiteral("FOLDER"), selectedFolderPath);
        return;
    }

    QMessageBox::warning(
        this,
        QStringLiteral("No Input"),
        QStringLiteral("Please select an image or folder first."));
}

void MainWindow::beginNewRun(
    const QString& sourceType,
    const QString& sourcePath)
{
    activeRunId =
        QUuid::createUuid().toString(QUuid::WithoutBraces);
    activeStoragePath = QDir(managedRunsRoot).filePath(activeRunId);
    activeRunStatus = QStringLiteral("CREATING");
    activeInferenceOptions = paramWidget->inferenceOptions();
    detectionStatus = DetectionStatus::PREPARING;
    claimInFlight = false;
    itemInFlight = false;
    persistenceInFlight = false;
    finishInFlight = false;

    resetStatusBar();
    DRWidget->model->clear();
    setInputSelectionEnabled(false);
    updateActionStates();
    refreshStatusBar(QStringLiteral("Preparing batch..."));

    NewInspectionRun run;
    run.runId = activeRunId;
    run.sourceType = sourceType;
    run.sourcePath = sourcePath;
    run.storagePath = activeStoragePath;
    run.storageOrigin = QStringLiteral("MANAGED");
    run.modelPath = loadedModelPath;
    run.appVersion = QCoreApplication::applicationVersion().isEmpty()
                         ? QStringLiteral("development")
                         : QCoreApplication::applicationVersion();
    run.reviewThreshold = activeInferenceOptions.reviewThreshold;
    run.ngThreshold = activeInferenceOptions.ngThreshold;
    run.heatmapAlpha = activeInferenceOptions.heatmapAlpha;
    run.startedAtMs = QDateTime::currentMSecsSinceEpoch();

    QMetaObject::invokeMethod(
        persistenceWorker,
        [worker = persistenceWorker, run]()
        {
            worker->createRun(run);
        },
        Qt::QueuedConnection);
}

void MainWindow::onRunCreated(const InspectionRunRecord& run)
{
    if (run.runId != activeRunId || closing)
    {
        return;
    }

    activeRunStatus = run.status;
    activeStoragePath = run.storagePath;
    totalImageCount = run.totalCount;
    detectionStatus = DetectionStatus::RUNNING;
    refreshStatusBar(
        QStringLiteral("Detecting: 0/%1").arg(totalImageCount));
    updateActionStates();
    startInferenceWorker(run.modelPath, activeInferenceOptions);
}

void MainWindow::startInferenceWorker(
    const QString& modelPath,
    const InferenceOptions& options)
{
    stopInferenceWorker();

    inferenceReady = false;
    inferenceThread = new QThread(this);
    imageWorker = new ImageWorker(modelPath, options);
    imageWorker->moveToThread(inferenceThread);

    connect(
        inferenceThread,
        &QThread::started,
        imageWorker,
        &ImageWorker::initialize);
    connect(
        inferenceThread,
        &QThread::finished,
        imageWorker,
        &QObject::deleteLater);
    connect(
        imageWorker,
        &ImageWorker::sigReady,
        this,
        &MainWindow::onInferenceReady);
    connect(
        imageWorker,
        &ImageWorker::sigInitializationFailed,
        this,
        &MainWindow::onInferenceInitializationFailed);
    connect(
        imageWorker,
        &ImageWorker::sigTaskFinished,
        this,
        &MainWindow::onTaskFinished);
    connect(
        imageWorker,
        &ImageWorker::sigTaskFailed,
        this,
        &MainWindow::onTaskFailed);
    inferenceThread->start();
}

void MainWindow::stopInferenceWorker()
{
    inferenceReady = false;
    if (inferenceThread == nullptr)
    {
        imageWorker = nullptr;
        return;
    }

    inferenceThread->quit();
    inferenceThread->wait();
    delete inferenceThread;
    inferenceThread = nullptr;
    imageWorker = nullptr;
}

void MainWindow::onInferenceReady()
{
    inferenceReady = true;
    requestNextItem();
}

void MainWindow::onInferenceInitializationFailed(
    const QString& errorMessage)
{
    stopInferenceWorker();
    detectionStatus = DetectionStatus::IDLE;
    activeRunStatus = QStringLiteral("RUNNING");
    updateActionStates();

    QMetaObject::invokeMethod(
        persistenceWorker,
        [worker = persistenceWorker, runId = activeRunId]()
        {
            worker->interruptRunningRun(runId);
        },
        Qt::QueuedConnection);

    QMessageBox::critical(
        this,
        QStringLiteral("Inference Initialization Failed"),
        errorMessage);
}

void MainWindow::requestNextItem()
{
    if (closing
        || detectionStatus != DetectionStatus::RUNNING
        || !inferenceReady
        || claimInFlight
        || itemInFlight
        || persistenceInFlight
        || finishInFlight
        || activeRunId.isEmpty())
    {
        return;
    }

    claimInFlight = true;
    QMetaObject::invokeMethod(
        persistenceWorker,
        [worker = persistenceWorker, runId = activeRunId]()
        {
            worker->claimNext(runId);
        },
        Qt::QueuedConnection);
}

void MainWindow::onItemClaimed(const ClaimedInspectionItem& item)
{
    if (item.runId != activeRunId || closing)
    {
        return;
    }
    claimInFlight = false;

    if (!item.found)
    {
        finishActiveRun();
        return;
    }

    itemInFlight = true;
    QMetaObject::invokeMethod(
        imageWorker,
        [worker = imageWorker, item]()
        {
            worker->processItem(item);
        },
        Qt::QueuedConnection);
}

void MainWindow::onTaskFinished(
    const ClaimedInspectionItem& item,
    const InferenceResult& result)
{
    if (item.runId != activeRunId || closing)
    {
        return;
    }
    itemInFlight = false;
    persistenceInFlight = true;

    QMetaObject::invokeMethod(
        persistenceWorker,
        [worker = persistenceWorker, item, result]()
        {
            worker->persistSuccess(item, result);
        },
        Qt::QueuedConnection);
}

void MainWindow::onTaskFailed(
    const ClaimedInspectionItem& item,
    const QString& errorMessage)
{
    if (item.runId != activeRunId || closing)
    {
        return;
    }
    itemInFlight = false;
    persistenceInFlight = true;

    QMetaObject::invokeMethod(
        persistenceWorker,
        [worker = persistenceWorker, item, errorMessage]()
        {
            worker->persistFailure(item, errorMessage);
        },
        Qt::QueuedConnection);
}

DetectionRecord MainWindow::recordFromPersisted(
    const PersistedInspectionResult& result) const
{
    DetectionRecord record;
    record.runId = result.runId;
    record.resultId = result.resultId;
    record.sequenceNo = result.sequenceNo;
    record.fileName =
        MainWindowUtils::displayFileName(result.sourceFilePath);
    record.sourceFileName = result.sourceFilePath;
    record.heatmapFileName = result.heatmapFilePath;
    record.status = QStringLiteral("SUCCESS");
    record.detectionLevel =
        MainWindowUtils::detectionLevelFromText(result.detectionLevel);
    record.hasDetectionResult = true;
    record.score = result.score;
    record.inferenceMS = static_cast<int>(result.inferenceMs);
    record.defectArea = static_cast<int>(result.defectArea);
    return record;
}

void MainWindow::onSuccessPersisted(
    const PersistedInspectionResult& result)
{
    if (result.runId != activeRunId || closing)
    {
        return;
    }
    persistenceInFlight = false;
    ++completedImageCount;
    totalScore += result.score;

    const DetectionLevel level =
        MainWindowUtils::detectionLevelFromText(result.detectionLevel);
    if (level == DetectionLevel::NG)
    {
        ++ngImageCount;
    }
    else if (level == DetectionLevel::REVIEW)
    {
        ++reviewImageCount;
    }
    else
    {
        ++okImageCount;
    }

    const DetectionRecord record = recordFromPersisted(result);
    DRWidget->model->appendResult(record);
    displayRecord(record);

    QDEBUG(
        QStringLiteral(
            "Inference persisted | File: %1 | Result: %2 | "
            "Score: %3 | Time: %4 ms | Defect Area: %5 px")
            .arg(result.sourceFilePath)
            .arg(result.detectionLevel)
            .arg(result.score, 0, 'f', 4)
            .arg(result.inferenceMs)
            .arg(result.defectArea));

    continueAfterPersistedItem();
}

void MainWindow::onFailurePersisted(
    const InspectionItemPageRow& item)
{
    if (closing || activeRunId.isEmpty())
    {
        return;
    }
    persistenceInFlight = false;
    ++completedImageCount;
    ++failedImageCount;

    DetectionRecord record;
    record.runId = activeRunId;
    record.resultId = item.resultId;
    record.sequenceNo = item.sequenceNo;
    record.fileName =
        MainWindowUtils::displayFileName(item.sourceFilePath);
    record.sourceFileName = item.sourceFilePath;
    record.status = QStringLiteral("FAILED");
    record.hasDetectionResult = false;
    record.errorMessage =
        item.errorMessage.value_or(QStringLiteral("Unknown Failure"));
    DRWidget->model->appendResult(record);

    QWARNING(
        QStringLiteral("Detection Failed | File: %1 | %2")
            .arg(record.sourceFileName, record.errorMessage));
    continueAfterPersistedItem();
}

void MainWindow::continueAfterPersistedItem()
{
    refreshStatusBar(
        QStringLiteral("Detecting: %1/%2")
            .arg(completedImageCount)
            .arg(totalImageCount));

    if (detectionStatus == DetectionStatus::PAUSEREQUEST)
    {
        enterPausedState();
        return;
    }
    requestNextItem();
}

void MainWindow::onStopActionTriggered()
{
    if (detectionStatus != DetectionStatus::RUNNING)
    {
        return;
    }

    detectionStatus = DetectionStatus::PAUSEREQUEST;
    refreshStatusBar(QStringLiteral("Finishing current image..."));
    updateActionStates();
    if (!claimInFlight && !itemInFlight && !persistenceInFlight)
    {
        enterPausedState();
    }
}

void MainWindow::enterPausedState()
{
    detectionStatus = DetectionStatus::PAUSED;
    refreshStatusBar(
        QStringLiteral("Paused: %1/%2")
            .arg(completedImageCount)
            .arg(totalImageCount));
    updateActionStates();
    QDEBUG(QStringLiteral("Detection Paused."));
}

void MainWindow::finishActiveRun()
{
    if (finishInFlight || activeRunId.isEmpty())
    {
        return;
    }
    finishInFlight = true;
    detectionStatus = DetectionStatus::FINALIZING;
    refreshStatusBar(QStringLiteral("Finalizing batch..."));
    updateActionStates();

    QMetaObject::invokeMethod(
        persistenceWorker,
        [worker = persistenceWorker, runId = activeRunId]()
        {
            worker->finishAsDraft(runId);
        },
        Qt::QueuedConnection);
}

void MainWindow::onRunDrafted(const InspectionRunRecord& run)
{
    if (run.runId != activeRunId || closing)
    {
        return;
    }

    finishInFlight = false;
    stopInferenceWorker();
    detectionStatus = DetectionStatus::IDLE;
    activeRunStatus = QStringLiteral("DRAFT");
    totalImageCount = run.totalCount;
    completedImageCount = run.completedCount;
    failedImageCount = run.failedCount;
    okImageCount = run.okCount;
    reviewImageCount = run.reviewCount;
    ngImageCount = run.ngCount;
    totalScore = run.scoreSum;
    refreshStatusBar(QStringLiteral("Draft ready"));
    updateActionStates();
    promptToSaveDraft();
}

void MainWindow::promptToSaveDraft()
{
    if (activeRunStatus != QStringLiteral("DRAFT") || closing)
    {
        return;
    }

    QMessageBox messageBox(this);
    messageBox.setWindowTitle(QStringLiteral("Detection Complete"));
    messageBox.setText(
        QStringLiteral(
            "The batch is complete. Save it to history or discard it?"));
    QPushButton* saveButton =
        messageBox.addButton(
            QStringLiteral("Save"),
            QMessageBox::AcceptRole);
    QPushButton* discardButton =
        messageBox.addButton(
            QStringLiteral("Discard"),
            QMessageBox::DestructiveRole);
    messageBox.addButton(
        QStringLiteral("Later"),
        QMessageBox::RejectRole);
    messageBox.exec();

    if (messageBox.clickedButton() == saveButton)
    {
        requestSaveDraft();
    }
    else if (messageBox.clickedButton() == discardButton)
    {
        requestDiscardDraft();
    }
}

void MainWindow::onSaveBatchActionTriggered()
{
    requestSaveDraft();
}

void MainWindow::requestSaveDraft()
{
    if (activeRunStatus != QStringLiteral("DRAFT")
        || activeRunId.isEmpty())
    {
        return;
    }

    bool accepted = false;
    const QString defaultName =
        QStringLiteral("Detection %1")
            .arg(QDateTime::currentDateTime().toString(
                QStringLiteral("yyyy-MM-dd HH-mm-ss")));
    const QString name = QInputDialog::getText(
        this,
        QStringLiteral("Save Detection Batch"),
        QStringLiteral("Batch name:"),
        QLineEdit::Normal,
        defaultName,
        &accepted);
    if (!accepted || name.trimmed().isEmpty())
    {
        closeAfterSave = false;
        return;
    }

    const QString note = QInputDialog::getMultiLineText(
        this,
        QStringLiteral("Batch Note"),
        QStringLiteral("Optional note:"),
        {},
        &accepted);
    if (!accepted)
    {
        closeAfterSave = false;
        return;
    }

    topToolBar->ui->saveBatchAction->setEnabled(false);
    topToolBar->ui->discardBatchAction->setEnabled(false);
    refreshStatusBar(QStringLiteral("Saving batch..."));
    QMetaObject::invokeMethod(
        persistenceWorker,
        [worker = persistenceWorker,
         runId = activeRunId,
         name,
         note]()
        {
            worker->saveDraft(runId, name, note);
        },
        Qt::QueuedConnection);
}

void MainWindow::onRunSaved(const QString& runId)
{
    if (runId != activeRunId)
    {
        return;
    }

    activeRunStatus = QStringLiteral("SAVED");
    detectionStatus = DetectionStatus::IDLE;
    setInputSelectionEnabled(true);
    refreshStatusBar(QStringLiteral("Saved"));
    updateActionStates();
    QDEBUG(QStringLiteral("Detection Batch Saved: %1").arg(runId));

    if (closeAfterSave)
    {
        closeAfterSave = false;
        QTimer::singleShot(0, this, &QWidget::close);
    }
}

void MainWindow::onDiscardBatchActionTriggered()
{
    if (activeRunStatus != QStringLiteral("DRAFT"))
    {
        return;
    }
    if (QMessageBox::question(
            this,
            QStringLiteral("Discard Draft"),
            QStringLiteral(
                "Discard this unsaved batch and delete its heatmaps?"))
        != QMessageBox::Yes)
    {
        return;
    }
    requestDiscardDraft();
}

void MainWindow::requestDiscardDraft()
{
    if (activeRunStatus != QStringLiteral("DRAFT")
        || activeRunId.isEmpty())
    {
        return;
    }
    topToolBar->ui->saveBatchAction->setEnabled(false);
    topToolBar->ui->discardBatchAction->setEnabled(false);
    refreshStatusBar(QStringLiteral("Discarding draft..."));
    QMetaObject::invokeMethod(
        persistenceWorker,
        [worker = persistenceWorker, runId = activeRunId]()
        {
            worker->discardDraft(runId);
        },
        Qt::QueuedConnection);
}

void MainWindow::onRunDiscarded(const QString& runId)
{
    if (runId != activeRunId)
    {
        return;
    }

    DRWidget->model->clear();
    clearActiveRun();
    resetStatusBar();
    detectionStatus = DetectionStatus::IDLE;
    setInputSelectionEnabled(true);
    updateActionStates();
    QDEBUG(QStringLiteral("Detection Draft Discarded: %1").arg(runId));
}

void MainWindow::onBatchManagerActionTriggered()
{
    requestBatchCatalog();
}

void MainWindow::requestBatchCatalog()
{
    if (!persistenceReady
        || detectionStatus != DetectionStatus::IDLE)
    {
        return;
    }
    QMetaObject::invokeMethod(
        persistenceWorker,
        [worker = persistenceWorker]()
        {
            worker->requestBatchCatalog();
        },
        Qt::QueuedConnection);
}

void MainWindow::onBatchCatalogReady(
    const InspectionRunRecords& runs)
{
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("Detection Batches"));
    dialog.resize(1050, 460);

    auto* layout = new QVBoxLayout(&dialog);
    auto* table = new QTableWidget(runs.size(), 7, &dialog);
    table->setHorizontalHeaderLabels(
        {
            QStringLiteral("Name"),
            QStringLiteral("Status"),
            QStringLiteral("Source"),
            QStringLiteral("Completed"),
            QStringLiteral("Failed"),
            QStringLiteral("Started"),
            QStringLiteral("Run ID")
        });
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    table->horizontalHeader()->setStretchLastSection(true);

    for (int row = 0; row < runs.size(); ++row)
    {
        const InspectionRunRecord& run = runs.at(row);
        table->setItem(
            row,
            0,
            new QTableWidgetItem(
                run.name.value_or(QStringLiteral("(interrupted)"))));
        table->setItem(row, 1, new QTableWidgetItem(run.status));
        table->setItem(
            row,
            2,
            new QTableWidgetItem(run.sourcePath));
        table->setItem(
            row,
            3,
            new QTableWidgetItem(
                QStringLiteral("%1/%2")
                    .arg(run.completedCount)
                    .arg(run.totalCount)));
        table->setItem(
            row,
            4,
            new QTableWidgetItem(QString::number(run.failedCount)));
        table->setItem(
            row,
            5,
            new QTableWidgetItem(
                QDateTime::fromMSecsSinceEpoch(run.startedAtMs)
                    .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))));
        table->setItem(row, 6, new QTableWidgetItem(run.runId));
    }
    layout->addWidget(table);

    auto* buttonLayout = new QHBoxLayout();
    auto* openButton =
        new QPushButton(QStringLiteral("Open / Resume"), &dialog);
    auto* deleteButton =
        new QPushButton(QStringLiteral("Delete Saved"), &dialog);
    auto* closeButton =
        new QPushButton(QStringLiteral("Close"), &dialog);
    buttonLayout->addStretch();
    buttonLayout->addWidget(openButton);
    buttonLayout->addWidget(deleteButton);
    buttonLayout->addWidget(closeButton);
    layout->addLayout(buttonLayout);

    QString selectedAction;
    QString selectedRunId;
    QString selectedStatus;
    auto captureSelection =
        [&]()
    {
        const int row = table->currentRow();
        if (row < 0 || row >= runs.size())
        {
            return false;
        }
        selectedRunId = runs.at(row).runId;
        selectedStatus = runs.at(row).status;
        return true;
    };

    connect(
        openButton,
        &QPushButton::clicked,
        &dialog,
        [&]()
        {
            if (captureSelection())
            {
                selectedAction = QStringLiteral("open");
                dialog.accept();
            }
        });
    connect(
        deleteButton,
        &QPushButton::clicked,
        &dialog,
        [&]()
        {
            if (!captureSelection())
            {
                return;
            }
            if (selectedStatus != QStringLiteral("SAVED"))
            {
                QMessageBox::information(
                    &dialog,
                    QStringLiteral("Delete Batch"),
                    QStringLiteral(
                        "Only saved batches can be deleted. "
                        "Interrupted batches can be resumed."));
                return;
            }
            if (QMessageBox::question(
                    &dialog,
                    QStringLiteral("Delete Saved Batch"),
                    QStringLiteral(
                        "Delete the selected batch, its database rows, "
                        "and managed heatmaps?"))
                == QMessageBox::Yes)
            {
                selectedAction = QStringLiteral("delete");
                dialog.accept();
            }
        });
    connect(
        closeButton,
        &QPushButton::clicked,
        &dialog,
        &QDialog::reject);
    connect(
        table,
        &QTableWidget::doubleClicked,
        &dialog,
        [&](const QModelIndex&)
        {
            if (captureSelection())
            {
                selectedAction = QStringLiteral("open");
                dialog.accept();
            }
        });

    dialog.exec();
    if (selectedAction == QStringLiteral("delete"))
    {
        requestDeleteSavedRun(selectedRunId);
    }
    else if (selectedAction == QStringLiteral("open"))
    {
        if (selectedStatus == QStringLiteral("INTERRUPTED"))
        {
            requestResumeRun(selectedRunId);
        }
        else
        {
            requestLoadSavedRun(selectedRunId);
        }
    }
}

void MainWindow::requestLoadSavedRun(const QString& runId)
{
    QMetaObject::invokeMethod(
        persistenceWorker,
        [worker = persistenceWorker, runId]()
        {
            worker->loadSavedRun(runId);
        },
        Qt::QueuedConnection);
}

void MainWindow::requestResumeRun(const QString& runId)
{
    if (detectionStatus != DetectionStatus::IDLE
        || activeRunStatus == QStringLiteral("DRAFT"))
    {
        return;
    }
    detectionStatus = DetectionStatus::PREPARING;
    refreshStatusBar(QStringLiteral("Preparing resume..."));
    updateActionStates();
    QMetaObject::invokeMethod(
        persistenceWorker,
        [worker = persistenceWorker, runId]()
        {
            worker->resumeInterruptedRun(runId);
        },
        Qt::QueuedConnection);
}

void MainWindow::requestDeleteSavedRun(const QString& runId)
{
    QMetaObject::invokeMethod(
        persistenceWorker,
        [worker = persistenceWorker, runId]()
        {
            worker->deleteSavedRun(runId);
        },
        Qt::QueuedConnection);
}

void MainWindow::onSavedRunDeleted(const QString& runId)
{
    if (runId == activeRunId)
    {
        DRWidget->model->clear();
        clearActiveRun();
        resetStatusBar();
    }
    updateActionStates();
    QDEBUG(QStringLiteral("Saved Batch Deleted: %1").arg(runId));
}

void MainWindow::onInterruptedRunsReady(
    const InspectionRunRecords& runs)
{
    if (runs.isEmpty() || closing)
    {
        return;
    }

    const InspectionRunRecord& latest = runs.first();
    if (QMessageBox::question(
            this,
            QStringLiteral("Resume Interrupted Detection"),
            QStringLiteral(
                "An interrupted batch has %1 of %2 items completed.\n"
                "Resume it now?")
                .arg(latest.completedCount)
                .arg(latest.totalCount))
        == QMessageBox::Yes)
    {
        requestResumeRun(latest.runId);
    }
}

DetectionRecord MainWindow::recordFromStoredItem(
    const InspectionRunRecord& run,
    const InspectionItemPageRow& item) const
{
    DetectionRecord record;
    record.runId = run.runId;
    record.resultId = item.resultId;
    record.sequenceNo = item.sequenceNo;
    record.fileName =
        MainWindowUtils::displayFileName(item.sourceFilePath);
    record.sourceFileName = item.sourceFilePath;
    record.status = item.status;
    record.hasDetectionResult =
        item.status == QStringLiteral("SUCCESS");
    if (item.detectionLevel.has_value())
    {
        record.detectionLevel =
            MainWindowUtils::detectionLevelFromText(
                item.detectionLevel.value());
    }
    record.score = item.score.value_or(0.0);
    record.inferenceMS =
        static_cast<int>(item.inferenceMs.value_or(0));
    record.defectArea =
        static_cast<int>(item.defectArea.value_or(0));
    record.errorMessage = item.errorMessage.value_or(QString());
    if (item.heatmapRelativePath.has_value())
    {
        record.heatmapFileName =
            QDir(run.storagePath).filePath(
                item.heatmapRelativePath.value());
    }
    return record;
}

void MainWindow::populateRunResults(
    const InspectionRunRecord& run,
    const InspectionItemRows& items)
{
    DRWidget->model->clear();
    for (const InspectionItemPageRow& item : items)
    {
        if (item.status == QStringLiteral("SUCCESS")
            || item.status == QStringLiteral("FAILED"))
        {
            DRWidget->model->appendResult(
                recordFromStoredItem(run, item));
        }
    }

    totalImageCount = run.totalCount;
    completedImageCount = run.completedCount;
    failedImageCount = run.failedCount;
    okImageCount = run.okCount;
    reviewImageCount = run.reviewCount;
    ngImageCount = run.ngCount;
    totalScore = run.scoreSum;
}

void MainWindow::onRunLoaded(
    const InspectionRunRecord& run,
    const InspectionItemRows& items,
    bool resumed)
{
    activeRunId = run.runId;
    activeRunStatus = run.status;
    activeStoragePath = run.storagePath;
    activeInferenceOptions.reviewThreshold = run.reviewThreshold;
    activeInferenceOptions.ngThreshold = run.ngThreshold;
    activeInferenceOptions.heatmapAlpha = run.heatmapAlpha;
    populateRunResults(run, items);

    if (!resumed)
    {
        detectionStatus = DetectionStatus::IDLE;
        refreshStatusBar(QStringLiteral("Viewing saved batch"));
        setInputSelectionEnabled(true);
        updateActionStates();
        return;
    }

    QString modelError;
    if (!inferenceEngine.loadModel(run.modelPath, &modelError))
    {
        activeRunStatus = QStringLiteral("RUNNING");
        detectionStatus = DetectionStatus::IDLE;
        QMetaObject::invokeMethod(
            persistenceWorker,
            [worker = persistenceWorker, runId = run.runId]()
            {
                worker->interruptRunningRun(runId);
            },
            Qt::QueuedConnection);
        QMessageBox::critical(
            this,
            QStringLiteral("Cannot Resume Batch"),
            QStringLiteral(
                "The stored model cannot be loaded.\n\n%1")
                .arg(modelError));
        updateActionStates();
        return;
    }

    loadedModelPath = run.modelPath;
    paramWidget->setInferenceOptions(activeInferenceOptions);
    detectionStatus = DetectionStatus::RUNNING;
    activeRunStatus = QStringLiteral("RUNNING");
    claimInFlight = false;
    itemInFlight = false;
    persistenceInFlight = false;
    finishInFlight = false;
    setInputSelectionEnabled(false);
    refreshStatusBar(
        QStringLiteral("Resuming: %1/%2")
            .arg(completedImageCount)
            .arg(totalImageCount));
    updateActionStates();
    startInferenceWorker(run.modelPath, activeInferenceOptions);
}

void MainWindow::onRunInterrupted(const QString& runId)
{
    if (runId == activeRunId)
    {
        activeRunStatus = QStringLiteral("INTERRUPTED");
        detectionStatus = DetectionStatus::IDLE;
        setInputSelectionEnabled(true);
        refreshStatusBar(QStringLiteral("Interrupted"));
        updateActionStates();
    }
}


void MainWindow::onCameraFrameReady(
    const DecodedCameraFrame& frame)
{
    if(frame.image.isNull())
    {
        QWARNING(
            QStringLiteral(
                "Camera frame %1 is empty")
                .arg(frame.header.sequence));
        return;
    }

    viewWidget->ui->originalGV->setImage(
        frame.image);

    viewWidget->setOriginalImagePath(
        QStringLiteral(
            "Camera_Frame_%1.jpg")
            .arg(frame.header.sequence));

    statusBar->ui->readyLabel->setText(
        QStringLiteral(
            "Camera frame %1 received")
            .arg(frame.header.sequence));
}


void MainWindow::onPersistenceOperationFailed(
    const QString& operation,
    const QString& contextId,
    const QString& errorMessage)
{
    QCRITICAL(
        QStringLiteral(
            "Persistence Operation Failed | Operation=%1 | Context=%2 | %3")
            .arg(operation, contextId, errorMessage));

    if (operation == QStringLiteral("create_run"))
    {
        clearActiveRun();
        detectionStatus = DetectionStatus::IDLE;
        setInputSelectionEnabled(true);
    }
    else if (operation == QStringLiteral("save_draft"))
    {
        activeRunStatus = QStringLiteral("DRAFT");
        refreshStatusBar(QStringLiteral("Draft save failed"));
    }
    else if (operation == QStringLiteral("resume_run"))
    {
        detectionStatus = DetectionStatus::IDLE;
        setInputSelectionEnabled(true);
    }
    else if (operation == QStringLiteral("claim_next")
             || operation == QStringLiteral("persist_success")
             || operation == QStringLiteral("persist_failure")
             || operation == QStringLiteral("finish_as_draft"))
    {
        claimInFlight = false;
        itemInFlight = false;
        persistenceInFlight = false;
        finishInFlight = false;
        stopInferenceWorker();
        if (!activeRunId.isEmpty())
        {
            QMetaObject::invokeMethod(
                persistenceWorker,
                [worker = persistenceWorker, runId = activeRunId]()
                {
                    worker->interruptRunningRun(runId);
                },
                Qt::QueuedConnection);
        }
    }

    updateActionStates();
    if (!closing)
    {
        QMessageBox::critical(
            this,
            QStringLiteral("Persistence Error"),
            QStringLiteral("%1\n\n%2").arg(operation, errorMessage));
    }
}

void MainWindow::onRecordClicked(const DetectionRecord& record)
{
    displayRecord(record);
}

void MainWindow::displayRecord(const DetectionRecord& record)
{
    const QImage originalImage(record.sourceFileName);
    if (!originalImage.isNull())
    {
        viewWidget->ui->originalGV->setImage(originalImage);
        viewWidget->setOriginalImagePath(record.sourceFileName);
    }
    else
    {
        viewWidget->ui->originalGV->clearImageInfo();
        QWARNING(
            QStringLiteral("Original Image is Missing: %1")
                .arg(record.sourceFileName));
    }

    if (record.hasDetectionResult)
    {
        const QImage heatmap(record.heatmapFileName);
        if (!heatmap.isNull())
        {
            viewWidget->ui->resultGV->setImage(heatmap);
        }
        else
        {
            viewWidget->ui->resultGV->clearImageInfo();
            QWARNING(
                QStringLiteral("Heatmap is Missing: %1")
                    .arg(record.heatmapFileName));
        }
        quickResWidget->ui->setNG(
            record.detectionLevel,
            record.score);
        quickResWidget->ui->setMetrics(
            record.inferenceMS,
            record.defectArea);
    }
    else
    {
        viewWidget->ui->resultGV->clearImageInfo();
        QWARNING(
            QStringLiteral("Failed Detection Item: %1")
                .arg(record.errorMessage));
    }
}

void MainWindow::resetStatusBar()
{
    totalImageCount = 0;
    completedImageCount = 0;
    failedImageCount = 0;
    ngImageCount = 0;
    okImageCount = 0;
    reviewImageCount = 0;
    totalScore = 0.0;
    refreshStatusBar(QStringLiteral("Ready"));
}

void MainWindow::refreshStatusBar(const QString& stateText)
{
    const qint64 successfulCount =
        okImageCount + reviewImageCount + ngImageCount;
    const double averageScore =
        successfulCount > 0
            ? totalScore / static_cast<double>(successfulCount)
            : 0.0;
    statusBar->ui->readyLabel->setText(stateText);
    statusBar->ui->imageCountLabel->setText(
        QStringLiteral("Images: %1").arg(completedImageCount));
    statusBar->ui->okLabel->setText(
        QStringLiteral("OK: %1").arg(okImageCount));
    statusBar->ui->reviewLabel->setText(
        QStringLiteral("Review: %1").arg(reviewImageCount));
    statusBar->ui->ngLabel->setText(
        QStringLiteral("NG: %1").arg(ngImageCount));
    statusBar->ui->avgScoreLabel->setText(
        QStringLiteral("AvgScore: %1 | Failed: %2")
            .arg(averageScore, 0, 'f', 3)
            .arg(failedImageCount));
}

void MainWindow::setInputSelectionEnabled(bool enabled)
{
    paramWidget->ui->selectImage->setEnabled(enabled);
    paramWidget->ui->selectFolder->setEnabled(enabled);
    topToolBar->ui->openImageAction->setEnabled(enabled);
    topToolBar->ui->openFolderAction->setEnabled(enabled);
}

void MainWindow::updateActionStates()
{
    const bool canStartNew =
        persistenceReady
        && detectionStatus == DetectionStatus::IDLE
        && activeRunStatus != QStringLiteral("DRAFT")
        && !loadedModelPath.isEmpty()
        && ((inputMode == InputMode::SINGLEIMAGE
             && !selectedImagePath.isEmpty())
            || (inputMode == InputMode::FOLDER
                && !selectedFolderPath.isEmpty()));
    topToolBar->ui->startAction->setEnabled(
        detectionStatus == DetectionStatus::PAUSED || canStartNew);
    topToolBar->ui->stopAction->setEnabled(
        detectionStatus == DetectionStatus::RUNNING);
    topToolBar->ui->saveBatchAction->setEnabled(
        activeRunStatus == QStringLiteral("DRAFT"));
    topToolBar->ui->discardBatchAction->setEnabled(
        activeRunStatus == QStringLiteral("DRAFT"));
    topToolBar->ui->batchManagerAction->setEnabled(
        persistenceReady
        && detectionStatus == DetectionStatus::IDLE
        && activeRunStatus != QStringLiteral("DRAFT"));
}

void MainWindow::clearActiveRun()
{
    activeRunId.clear();
    activeRunStatus.clear();
    activeStoragePath.clear();
    claimInFlight = false;
    itemInFlight = false;
    persistenceInFlight = false;
    finishInFlight = false;
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (activeRunStatus == QStringLiteral("DRAFT"))
    {
        const QMessageBox::StandardButton answer =
            QMessageBox::question(
                this,
                QStringLiteral("Unsaved Detection Batch"),
                QStringLiteral(
                    "Save this batch before closing?\n"
                    "Choosing No discards the database rows and heatmaps."),
                QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
                QMessageBox::Yes);
        if (answer == QMessageBox::Cancel)
        {
            event->ignore();
            return;
        }
        if (answer == QMessageBox::Yes)
        {
            closeAfterSave = true;
            requestSaveDraft();
            event->ignore();
            return;
        }

        closing = true;
        discardActiveDraftBlocking();
        event->accept();
        return;
    }

    const bool activeDetection =
        detectionStatus == DetectionStatus::PREPARING
        || detectionStatus == DetectionStatus::RUNNING
        || detectionStatus == DetectionStatus::PAUSEREQUEST
        || detectionStatus == DetectionStatus::PAUSED
        || detectionStatus == DetectionStatus::FINALIZING;
    if (activeDetection)
    {
        if (QMessageBox::question(
                this,
                QStringLiteral("Detection in Progress"),
                QStringLiteral(
                    "Close now and keep this batch for resume next time?"))
            != QMessageBox::Yes)
        {
            event->ignore();
            return;
        }

        closing = true;
        stopInferenceWorker();
        interruptActiveRunBlocking();
        event->accept();
        return;
    }

    closing = true;
    event->accept();
}

void MainWindow::interruptActiveRunBlocking()
{
    if (!persistenceReady
        || persistenceWorker == nullptr
        || persistenceThread == nullptr
        || !persistenceThread->isRunning()
        || activeRunId.isEmpty()
        || activeRunStatus != QStringLiteral("RUNNING"))
    {
        return;
    }

    QMetaObject::invokeMethod(
        persistenceWorker,
        [worker = persistenceWorker, runId = activeRunId]()
        {
            worker->interruptRunningRun(runId);
        },
        Qt::BlockingQueuedConnection);
}

void MainWindow::discardActiveDraftBlocking()
{
    if (!persistenceReady
        || persistenceWorker == nullptr
        || persistenceThread == nullptr
        || !persistenceThread->isRunning()
        || activeRunId.isEmpty()
        || activeRunStatus != QStringLiteral("DRAFT"))
    {
        return;
    }

    QMetaObject::invokeMethod(
        persistenceWorker,
        [worker = persistenceWorker, runId = activeRunId]()
        {
            worker->discardDraft(runId);
        },
        Qt::BlockingQueuedConnection);
}
