#include "mainwindow.h"


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , topToolBar(new ToolBar())
    , modelWidget(new ModelWidget())
    , paramWidget(new ParamWidget())
    , centralWidget(new QWidget())
    , mainLayout(new QHBoxLayout())
    , leftLayout(new QVBoxLayout())
    , rightLayout(new QVBoxLayout())
    , dataLayout(new QVBoxLayout())
    , viewLayout(new QHBoxLayout())
{
    topToolBar->ui->setupUI(this);
    setupUI();
}

MainWindow::~MainWindow() {}


void MainWindow::setupUI()
{
    resize(1700,850);
    setWindowTitle("Gluttony Vision");
    setWindowIcon(QIcon(":/Detective.png"));
    addToolBar(Qt::TopToolBarArea,topToolBar->ui->topToolBar);

    centralWidget->setLayout(mainLayout);
    mainLayout->addLayout(leftLayout);
    mainLayout->addLayout(rightLayout);
    leftLayout->addLayout(dataLayout);
    leftLayout->addLayout(viewLayout);

    dataLayout->addWidget(modelWidget);
    dataLayout->addWidget(paramWidget);

    this->setCentralWidget(centralWidget);

}
