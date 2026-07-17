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
    connect(topToolBar->ui->openImageAction,&QAction::triggered,paramWidget,&ParamWidget::onSelectImageButtonClicked);
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

    connect(paramWidget,&ParamWidget::sigImageSlected,this,&MainWindow::onImageSelected);
    connect(topToolBar->ui->startAction,&QAction::triggered,this,&MainWindow::onStartActionTriggered);
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

}


void MainWindow::onModelSelected(const QString& xmlPath)
{
    QString errorMessage;

    if (!inferenceEngine.loadModel(xmlPath, &errorMessage)) {
        QWARNING(errorMessage);
        QMessageBox::critical(this, "Model Loading Failed", errorMessage);
        return;
    }

    QDEBUG(QString("Model loaded successfully: %1").arg(xmlPath));
    QDEBUG("Device: CPU");

    // 将模型端口信息写到 LogWidget。
    for (const QString& input : inferenceEngine.inputDescriptions()) {
        QDEBUG(input);
    }

    for (const QString& output : inferenceEngine.outputDescriptions()) {
        QDEBUG(output);
    }
}


void MainWindow::onImageSelected(const QImage image, const QString imagePath)
{
    selectedImage = image;
    slectedImagePath = imagePath;
}


void MainWindow::onStartActionTriggered()
{
    if(!inferenceEngine.isLoaded())
    {
        const QString message = "Please Load an OpenVINO Model Before Starting Inference.";
        QWARNING(message);
        QMessageBox::warning(this,"Model Not Loaded",message);
        return ;
    }
    if(selectedImage.isNull())
    {
        const QString message = "Please Select an Image Before Starting Inference.";
        QWARNING(message);
        QMessageBox::warning(this,"Image Not Loaded",message);
        return ;
    }

    InferenceResult result;
    QString errorMessage;
    if(!inferenceEngine.inferImage(selectedImage,&result,&errorMessage))
    {
        QWARNING(errorMessage);
        QMessageBox::critical(this,"Inference Failed",errorMessage);
        return ;
    }

    quickResWidget->ui->setNG(result.isNg,result.score);
    quickResWidget->ui->setMetrics(result.inferenceMs,result.defectArea);

    if(!result.heatmapOverlay.isNull())
    {
        viewWidget->ui->resultGV->setImage(result.heatmapOverlay);
    }

    const QString status = result.isNg ? "NG" : "OK";

    QDEBUG(
        QString(
            "Inference completed | File: %1 | Result: %2 | "
            "Score: %3 | Time: %4 ms | Defect Area: %5 px"
            )
            .arg(slectedImagePath)
            .arg(status)
            .arg(result.score, 0, 'f', 4)
            .arg(result.inferenceMs)
            .arg(result.defectArea)
        );
}
