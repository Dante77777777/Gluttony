#include "mainwindow.h"


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , topToolBar(new ToolBar())
    , statusBar(new StatusBar)
    , modelWidget(new ModelWidget(this))
    , viewWidget(new ViewWidget(this))
    , paramWidget(new ParamWidget(viewWidget,this))
    , DRWidget(new DetectionResult())
    , quickResWidget(new QuickResWidget())
    , logWidget(new LogWidget())
    , centralWidget(new QWidget())
    , mainLayout(new QHBoxLayout())
    , leftLayout(new QVBoxLayout())
    , rightLayout(new QVBoxLayout())
    , dataLayout(new QVBoxLayout())
    , viewLayout(new QHBoxLayout())
    , leftTopLayout(new QHBoxLayout())
{

    statusBar->ui->setupUI(this);
    setupToolBar();
    setupUI();
}

MainWindow::~MainWindow() {}


void MainWindow::setupToolBar()
{
    topToolBar->ui->setupUI(this);
    connect(topToolBar->ui->openImageAction,&QAction::triggered,paramWidget,&::ParamWidget::onSelectImageButtonClicked);
    connect(topToolBar->ui->openFolderAction,&QAction::triggered,paramWidget,&ParamWidget::onSelectFolderButtonClicked);

    connect(topToolBar->ui->zoomOutAction,&QAction::triggered,viewWidget->ui->originalWidget,[this](){
        viewWidget->ui->originalGV->onZoom(0.9);
    });
    connect(topToolBar->ui->zoomOutAction,&QAction::triggered,viewWidget->ui->resultWidget,[this](){
        viewWidget->ui->resultGV->onZoom(0.9);
    });

    connect(topToolBar->ui->zoomInAction,&QAction::triggered,viewWidget->ui->originalWidget,[this](){
        viewWidget->ui->originalGV->onZoom(1.1);
    });
    connect(topToolBar->ui->zoomInAction,&QAction::triggered,viewWidget->ui->resultWidget,[this](){
        viewWidget->ui->resultGV->onZoom(1.1);
    });

    connect(topToolBar->ui->fitAction,&QAction::triggered,viewWidget->ui->originalGV,&GraphicsView::onFit);
    connect(topToolBar->ui->fitAction,&QAction::triggered,viewWidget->ui->resultGV,&GraphicsView::onFit);

    connect(topToolBar->ui->rotateLeftAction,&QAction::triggered,viewWidget->ui->originalGV,&GraphicsView::onRotateLeft);
    connect(topToolBar->ui->rotateLeftAction,&QAction::triggered,viewWidget->ui->resultGV,&GraphicsView::onRotateLeft);

    connect(topToolBar->ui->rotateRightAction,&QAction::triggered,viewWidget->ui->originalGV,&GraphicsView::onRotateRight);
    connect(topToolBar->ui->rotateRightAction,&QAction::triggered,viewWidget->ui->resultGV,&GraphicsView::onRotateRight);

    connect(modelWidget,&ModelWidget::sigModelSelect,this,&MainWindow::onModelSelected);

    connect(paramWidget,&ParamWidget::sigImageSelected,this,&MainWindow::onImageSelected);
    connect(paramWidget,&ParamWidget::sigFolderSelected,this,&MainWindow::onFolderSelected);

    connect(topToolBar->ui->startAction,&QAction::triggered,this,&MainWindow::onStartActionTriggered);

    connect(topToolBar->ui->stopAction,&QAction::triggered,this,&MainWindow::onStopActionTriggered);
}


void MainWindow::setupUI()
{
    resize(1700,950);
    setWindowTitle("Gluttony Vision");
    setWindowIcon(QIcon(":/Detective.png"));
    addToolBar(Qt::TopToolBarArea,topToolBar->ui->topToolBar);
    setStatusBar(statusBar->ui->statusBar);

    centralWidget->setLayout(mainLayout);
    mainLayout->addLayout(leftLayout,5);
    mainLayout->addLayout(rightLayout,1);
    leftLayout->addLayout(leftTopLayout,5);
    leftLayout->addWidget(DRWidget,2);
    leftTopLayout->addLayout(dataLayout);
    leftTopLayout->addLayout(viewLayout);
    leftTopLayout->setAlignment(dataLayout, Qt::AlignTop);

    rightLayout->addWidget(quickResWidget,1);
    rightLayout->addWidget(logWidget,1);

    dataLayout->addWidget(modelWidget);
    dataLayout->addWidget(paramWidget);
    viewLayout->addWidget(viewWidget);

    this->setCentralWidget(centralWidget);

    connect(DRWidget,&DetectionResult::sigRecord,this,&MainWindow::onRecordClicked);

}


void MainWindow::onModelSelected(const QString& xmlPath)
{
    QString errorMessage;

    if (!inferenceEngine.loadModel(xmlPath, &errorMessage)) {
        QWARNING(errorMessage);
        QMessageBox::critical(this, "Model Loading Failed", errorMessage);
        return;
    }

    loadedModelPath = xmlPath;

    QDEBUG(QString("Model loaded successfully: %1").arg(xmlPath));
    QDEBUG("Device: CPU");

    // 将模型端口信息写到 LogWidget。
    for (const QString& input : inferenceEngine.inputDescriptions()) {
        QDEBUG(input);
    }

    for (const QString& output : inferenceEngine.outputDescriptions()) {
        QDEBUG(output);
    }

    if(!slectedImagePath.isNull() || !selectedFolderPath.isNull())
    {
        topToolBar->ui->startAction->setEnabled(true);
    }
}


void MainWindow::onImageSelected(const QImage image, const QString imagePath)
{
    selectedImage = image;
    slectedImagePath = imagePath;
    if(!loadedModelPath.isNull())
    {
        topToolBar->ui->startAction->setEnabled(true);
    }
    inputMode = InputMode::SINGLEIMAGE;
    selectedFolderPath.clear();
}


void MainWindow::onFolderSelected(const QString& folderPath)
{
    selectedFolderPath = folderPath;
    if(!loadedModelPath.isNull())
    {
        topToolBar->ui->startAction->setEnabled(true);
    }
    inputMode = InputMode::FOLDER;
}


void MainWindow::onStartActionTriggered()
{
    if(detectionStatus == DetectionStatus::PAUSED)
    {
        detectionStatus = DetectionStatus::RUNNING;
        imageQueue->resume();
        topToolBar->ui->startAction->setEnabled(false);
        topToolBar->ui->stopAction->setEnabled(true);

        statusBar->ui->readyLabel->setText(QString("Detecting: %1/%2").arg(completedImageCount).arg(totalImageCount));
        return ;
    }

    if(detectionStatus != DetectionStatus::IDLE)
    {
        return ;
    }

    if(detectionStatus == DetectionStatus::IDLE)
    {
        detectionStatus = DetectionStatus::RUNNING;
        topToolBar->ui->startAction->setEnabled(false);
        topToolBar->ui->stopAction->setEnabled(true);
    }

    if(!inferenceEngine.isLoaded())
    {
        const QString message = "Please Load an OpenVINO Model Before Starting Inference.";
        QWARNING(message);
        QMessageBox::warning(this,"Model Not Loaded",message);
        return ;
    }

    // if(inferenceThread != nullptr)
    // {
    //     QMessageBox::information(this, "Detection Running","A Detection Task is Already Running.");
    //     return ;
    // }

    if(inputMode == InputMode::SINGLEIMAGE)
    {
        if(slectedImagePath.isEmpty())
        {
            //QWARNING("hello");
            return ;
        }
        startDetection({slectedImagePath});
        return ;
    }

    if(inputMode == InputMode::FOLDER)
    {
        if(selectedFolderPath.isEmpty())
        {
            return ;
        }
        startFolderDetection(selectedFolderPath);
        return ;
    }

    QMessageBox::warning(
        this,
        "No Input",
        "Please Select an Image or Folder First."
        );
}


void MainWindow::onStopActionTriggered()
{
    if(detectionStatus != DetectionStatus::RUNNING || !imageQueue)
    {
        return ;
    }

    imageQueue->pause();
    detectionStatus = DetectionStatus::PAUSEREQUEST;
    QDEBUG("Detection is Pausing");
    topToolBar->ui->startAction->setEnabled(false);
    topToolBar->ui->stopAction->setEnabled(false);

    statusBar->ui->readyLabel->setText(QString( "Finishing Current Image..."));
}


void MainWindow::onTaskFinished(const QString& filePath, const QImage& sourceImage, const InferenceResult& result)
{
    ++completedImageCount;
    // 显示当前完成的原图与热力图。
    viewWidget->ui->originalGV->setImage(sourceImage);
    viewWidget->setOriginalImagePath(filePath);
    viewWidget->ui->resultGV->setImage(result.heatmapOverlay);

    // 右侧当前结果。
    quickResWidget->ui->setNG(result.isNg, result.score);
    quickResWidget->ui->setMetrics(
        result.inferenceMs,
        result.defectArea
        );

    // 表格增加一行。
    DetectionRecord record;
    record.fileName = QString("%1/%2").arg(QFileInfo(filePath).dir().dirName()).arg(QFileInfo(filePath).fileName());
    record.isNG = result.isNg;
    record.score = result.score;
    record.inferenceMS = result.inferenceMs;
    record.defectArea = result.defectArea;
    record.sourceFileName = filePath;
    record.heatmapOverlay = result.heatmapOverlay;

    DRWidget->model->appendResult(record);

    QDEBUG(
        QString("Inference completed | File: %1 | Result: %2 | "
                "Score: %3 | Time: %4 ms | Defect Area: %5 px")
            .arg(filePath)
            .arg(result.isNg ? "NG" : "OK")
            .arg(result.score, 0, 'f', 4)
            .arg(result.inferenceMs)
            .arg(result.defectArea)
        );

    if(result.isNg)
    {
        ngImageCount++;
    }
    else
    {
        okImageCount++;
    }
    totalScore += result.score;
    refreshStatusBar(QString("Detecting: %1/%2").arg(completedImageCount).arg(totalImageCount));
}


void MainWindow::onTaskFailed(const QString& filePath, const QString& errorMessage)
{
    QWARNING(QString("File: %1 | %2").arg(filePath, errorMessage));
}


void MainWindow::onBatchFinished()
{
    QDEBUG("Batch inference finished.");

    detectionStatus = DetectionStatus::IDLE;
    topToolBar->ui->startAction->setEnabled(true);
    topToolBar->ui->stopAction->setEnabled(false);
    statusBar->ui->readyLabel->setText("Ready");

    producerThread = nullptr;
    inferenceThread = nullptr;
    imageProducer = nullptr;
    imageWorker = nullptr;
    imageQueue.reset();
    paramWidget->ui->selectImage->setEnabled(true);
    paramWidget->ui->selectFolder->setEnabled(true);
    refreshStatusBar(QString("Ready"));
}


void MainWindow::startWorkers()
{
    resetStatusBar();
    DRWidget->model->clear();
    imageQueue = std::make_shared<ImageQueue>(10);
    producerThread = new QThread(this);
    inferenceThread = new QThread(this);

    imageProducer = new ImageProducer(imageQueue);
    imageWorker = new ImageWorker(imageQueue,loadedModelPath);

    imageProducer->moveToThread(producerThread);
    imageWorker->moveToThread(inferenceThread);

    connect(inferenceThread,&QThread::started,imageWorker,&ImageWorker::run);
    connect(imageWorker,&ImageWorker::sigTaskFinished,this,&MainWindow::onTaskFinished);
    connect(imageWorker,&ImageWorker::sigTaskFailed,this,&MainWindow::onTaskFailed);
    connect(imageWorker,&ImageWorker::sigFinished,this,&MainWindow::onBatchFinished);
    connect(imageWorker,&ImageWorker::sigPaused,this,&MainWindow::onDetectionPaused);

    connect(imageProducer,&ImageProducer::sigFinish,producerThread,&QThread::quit);
    connect(imageWorker,&ImageWorker::sigFinished,inferenceThread,&QThread::quit);
    connect(producerThread,&QThread::finished,imageProducer,&QObject::deleteLater);
    connect(inferenceThread,&QThread::finished,imageWorker,&QObject::deleteLater);

    connect(imageProducer,&ImageProducer::sigTotalReady,this,[this](int total){
        this->totalImageCount = total;
        statusBar->ui->readyLabel->setText(QString("Detecting: 0/%1").arg(totalImageCount));
    });
}


void MainWindow::startDetection(const QStringList& singleFiles)
{
    startWorkers();
    connect(producerThread,&QThread::started,imageProducer,[this,singleFiles](){
        imageProducer->produceFiles(singleFiles);
    });
    paramWidget->ui->selectImage->setEnabled(false);
    paramWidget->ui->selectFolder->setEnabled(false);
    producerThread->start();
    inferenceThread->start();
}


void MainWindow::startFolderDetection(const QString& folderPath)
{
    startWorkers();
    connect(producerThread,&QThread::started,imageProducer,[this,folderPath](){
        imageProducer->produceFolder(folderPath);
    });
    paramWidget->ui->selectImage->setEnabled(false);
    paramWidget->ui->selectFolder->setEnabled(false);
    producerThread->start();
    inferenceThread->start();
}



void MainWindow::resetStatusBar()
{
    completedImageCount = 0;
    totalImageCount = 0;
    ngImageCount = 0;
    okImageCount = 0;
    totalScore = 0.0;

    refreshStatusBar("Detecting...");
}


void MainWindow::refreshStatusBar(const QString& stateText)
{
    const double avgScore = completedImageCount > 0 ? totalScore/completedImageCount : 0.0;
    statusBar->ui->readyLabel->setText(stateText);
    statusBar->ui->imageCountLabel->setText(QString("Images: %1").arg(completedImageCount));
    statusBar->ui->okLabel->setText(QString("OK: %1").arg(okImageCount));
    statusBar->ui->ngLabel->setText(QString("NG: %1").arg(ngImageCount));
    statusBar->ui->avgScoreLabel->setText(QString("AvgScore: %1").arg(avgScore,0,'f',3));
}


void MainWindow::onDetectionPaused()
{
    if(detectionStatus != DetectionStatus::PAUSEREQUEST)
    {
        return ;
    }

    detectionStatus = DetectionStatus::PAUSED;
    topToolBar->ui->startAction->setEnabled(true);
    topToolBar->ui->stopAction->setEnabled(false);
    QWARNING("Detection Paused!");
    statusBar->ui->readyLabel->setText(QString("Detecting: %1/%2").arg(completedImageCount).arg(totalImageCount));
}


void MainWindow::onRecordClicked(const DetectionRecord& record)
{
    QImage originalImage(record.sourceFileName);
    if(originalImage.isNull())
    {
        QWARNING(QString("Original Image:%1 is Null").arg(record.sourceFileName));
        QMessageBox::warning(
            this,
            "Image Not Found",
            "The original image cannot be loaded."
            );
        return ;
    }
    viewWidget->ui->originalGV->setImage(originalImage);
    viewWidget->setOriginalImagePath(record.sourceFileName);

    if(!record.heatmapOverlay.isNull())
    {
        viewWidget->ui->resultGV->setImage(record.heatmapOverlay);
    }

    quickResWidget->ui->setNG(record.isNG,record.score);
    quickResWidget->ui->setMetrics(record.inferenceMS,record.defectArea);
}
