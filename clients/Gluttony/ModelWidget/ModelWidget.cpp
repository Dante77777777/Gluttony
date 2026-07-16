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

    QString xmlModelPath = QFileDialog::getOpenFileName(this,"Select OpenVINO XML",currentDir,"OpenVINO Model (*.xml)");

    if(xmlModelPath.isEmpty())
    {
        QMessageBox::critical(this,"Error","Slected Model Path is Empty!");
        QFATAL("Slected Model Path is Empty!");
        return ;
    }
    ui->models->addItem(QFileInfo(xmlModelPath).fileName(),xmlModelPath);
    QDEBUG("Model is Selected Sucessfully!");
    emit sigModelSelect(xmlModelPath);
}
