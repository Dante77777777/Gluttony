#include "ParamWidget.h"

ParamWidget::ParamWidget(ViewWidget* viewWidget_, QWidget* parent)
    : QWidget(parent)
    , ui(new ParamWidgetUI())
    , viewWidget(viewWidget_)
{
    this->setObjectName("ParamWidget");
    setAttribute(Qt::WA_StyledBackground, true);
    setupUI();
}

ParamWidget::~ParamWidget()
{

}

void ParamWidget::setupUI()
{
    ui->setupUI(this);
    this->setFixedWidth(300);
    this->setFixedHeight(420);
    connect(ui->selectImage,&QPushButton::clicked,this,&ParamWidget::onSelectImageButtonClicked);
    connect(ui->selectFolder,&QPushButton::clicked,this,&ParamWidget::onSelectFolderButtonClicked);
}


void ParamWidget::onSelectImageButtonClicked()
{
    QString curreentDir = QDir::currentPath();
    QString imagePath = QFileDialog::getOpenFileName(this,"Select Image",curreentDir,"*.png *.jepg *.jpg");

    if(imagePath.isEmpty())
    {
        QCRITICAL("Empty Image Path");
        QMessageBox::critical(this,"Error","This Image Path is Empty!");
        return ;
    }
    QImage image = QImage(imagePath);
    viewWidget->ui->originalGV->setImage(image);
    ui->imagePath->setText(imagePath);
    emit sigImageSlected(image,imagePath);
    QDEBUG("Image Selected From [ " + imagePath + " ]");
}


void ParamWidget::onSelectFolderButtonClicked()
{
    QString currentDir = QDir::currentPath();
    QString folderPath = QFileDialog::getExistingDirectory(this,"Select Image Folder",currentDir,QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

    if(folderPath.isEmpty())
    {
        QCRITICAL("Empty Image Folder");
        QMessageBox::critical(this,"Error","This Image Folder URL is Invalid!");
    }

    ui->imagePath->setText(folderPath);
    QDEBUG("Image Folder Selected From [ " + folderPath + " ]");
}
