#include "ModelWidget.h"

ModelWidget::ModelWidget(QWidget* parent)
    : QWidget(parent)
    , ui(new ModelWidgetUI())
{
    this->setObjectName("ModelWidget");
    setAttribute(Qt::WA_StyledBackground, true);
    setupUI();
}

ModelWidget::~ModelWidget()
{

}

void ModelWidget::setupUI()
{
    ui->setupUI(this);
    this->setFixedWidth(300);
    this->setFixedHeight(130);
    connect(ui->loadModelButton,
            &QPushButton::clicked,
            this,
            &ModelWidget::onSelectModelButtonClicked);
}


void ModelWidget::onSelectModelButtonClicked()
{
    QString currentDir = QDir::currentPath();

    const QString modelPath = QFileDialog::getOpenFileName(
        this,
        "Select Model",
        currentDir,
        "Supported Models (*.xml *.onnx);;OpenVINO IR (*.xml);;ONNX Model (*.onnx)"
        );

    if(modelPath.isEmpty())
    {
        return ;
    }
    ui->models->addItem(QFileInfo(modelPath).fileName(),modelPath);
    QDEBUG("Model is Selected Successfully!");
    emit sigModelSelect(modelPath);
}
