#include "ParamWidget.h"
#include <QSignalBlocker>

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
    this->setFixedHeight(500);
    connect(ui->selectImage,&QPushButton::clicked,this,&ParamWidget::onSelectImageButtonClicked);
    connect(ui->selectFolder,&QPushButton::clicked,this,&ParamWidget::onSelectFolderButtonClicked);


    constexpr double gap = 0.01;
    auto updateReviewConstraint = [this](double reviewThreshold)
    {
        const double minNgThreshold = qMin(1.0, reviewThreshold + gap);
        ui->ngThresholdValue->setMinimum(minNgThreshold);
        ui->ngThresholdSlider->setMinimum(qRound(minNgThreshold * 100.0));
        if(ui->ngThresholdValue->value() < minNgThreshold)
        {
            QSignalBlocker spinBlocker(ui->ngThresholdValue);
            QSignalBlocker sliderBlocker(ui->ngThresholdSlider);

            ui->ngThresholdValue->setValue(minNgThreshold);
            ui->ngThresholdSlider->setValue(qRound(minNgThreshold*100.0));
        }
    };

    auto updateNgConstraint = [this](double ngThreshold)
    {
        const double maxReviewThreshold = qMax(0.0,ngThreshold-gap);
        ui->reviewThresholdValue->setMaximum(maxReviewThreshold);
        ui->reviewThresholdSlider->setMaximum(qRound(maxReviewThreshold*100.0));

        if(ui->reviewThresholdValue->value() > maxReviewThreshold)
        {
            QSignalBlocker spinBlocker(ui->reviewThresholdValue);
            QSignalBlocker sliderBlocker(ui->reviewThresholdSlider);

            ui->reviewThresholdValue->setValue(maxReviewThreshold);
            ui->reviewThresholdSlider->setValue(qRound(maxReviewThreshold * 100.0));
        }
    };

    auto bindSpinBoxAndSlider = [this](QDoubleSpinBox* spinBox,
                                       QSlider* slider) {
        connect(spinBox,
                QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this,
                [slider](double value) {
                    QSignalBlocker blocker(slider);
                    slider->setValue(qRound(value * 100.0));
                });

        connect(slider,
                &QSlider::valueChanged,
                this,
                [spinBox](int value) {
                    QSignalBlocker blocker(spinBox);
                    spinBox->setValue(value / 100.0);
                });
    };

    bindSpinBoxAndSlider(ui->reviewThresholdValue,
                         ui->reviewThresholdSlider);
    bindSpinBoxAndSlider(ui->ngThresholdValue,
                         ui->ngThresholdSlider);
    bindSpinBoxAndSlider(ui->alphaValue,
                         ui->alphaSlider);

    connect(ui->reviewThresholdValue,QOverload<double>::of(&QDoubleSpinBox::valueChanged),this,updateReviewConstraint);

    connect(ui->reviewThresholdSlider,&QSlider::valueChanged,this,[updateReviewConstraint](int value){
        updateReviewConstraint(value / 100.0);
    });

    connect(ui->ngThresholdValue,QOverload<double>::of(&QDoubleSpinBox::valueChanged),this,updateNgConstraint);

    connect(ui->ngThresholdSlider,&QSlider::valueChanged,this,[updateNgConstraint](int value){
        updateNgConstraint(value / 100.0);
    });

    updateReviewConstraint(ui->reviewThresholdValue->value());

    updateNgConstraint(ui->ngThresholdValue->value());
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
    viewWidget->setOriginalImagePath(imagePath);
    ui->imagePath->setText(imagePath);
    emit sigImageSelected(image,imagePath);
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
        return ;
    }

    ui->imagePath->setText(folderPath);
    emit sigFolderSelected(folderPath);
    QDEBUG("Image Folder Selected From [ " + folderPath + " ]");
}


InferenceOptions ParamWidget::inferenceOptions() const
{
    InferenceOptions options;
    options.heatmapAlpha = ui->alphaValue->value();
    options.ngThreshold = ui->ngThresholdValue->value();
    options.reviewThreshold = ui->reviewThresholdValue->value();
    return options;
}

void ParamWidget::setInferenceOptions(const InferenceOptions& options)
{
    const double reviewThreshold =
        qBound(0.0, options.reviewThreshold, 0.99);
    const double ngThreshold =
        qBound(reviewThreshold + 0.01, options.ngThreshold, 1.0);
    const double heatmapAlpha =
        qBound(0.0, options.heatmapAlpha, 1.0);

    const QSignalBlocker reviewSpinBlocker(ui->reviewThresholdValue);
    const QSignalBlocker reviewSliderBlocker(ui->reviewThresholdSlider);
    const QSignalBlocker ngSpinBlocker(ui->ngThresholdValue);
    const QSignalBlocker ngSliderBlocker(ui->ngThresholdSlider);
    const QSignalBlocker alphaSpinBlocker(ui->alphaValue);
    const QSignalBlocker alphaSliderBlocker(ui->alphaSlider);

    ui->reviewThresholdValue->setMaximum(ngThreshold - 0.01);
    ui->reviewThresholdSlider->setMaximum(
        qRound((ngThreshold - 0.01) * 100.0));
    ui->ngThresholdValue->setMinimum(reviewThreshold + 0.01);
    ui->ngThresholdSlider->setMinimum(
        qRound((reviewThreshold + 0.01) * 100.0));

    ui->reviewThresholdValue->setValue(reviewThreshold);
    ui->reviewThresholdSlider->setValue(qRound(reviewThreshold * 100.0));
    ui->ngThresholdValue->setValue(ngThreshold);
    ui->ngThresholdSlider->setValue(qRound(ngThreshold * 100.0));
    ui->alphaValue->setValue(heatmapAlpha);
    ui->alphaSlider->setValue(qRound(heatmapAlpha * 100.0));
}
