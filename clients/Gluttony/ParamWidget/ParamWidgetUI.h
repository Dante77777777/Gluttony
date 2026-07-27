#include <QLabel>
#include <QBoxLayout>
#include <QPushButton>
#include <QComboBox>
#include <QWidget>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QSlider>
#include <QFrame>

class ParamWidgetUI
{
public:
    QVBoxLayout* mainLayout;
    QLabel* title;
    QWidget* contentWidget;
    QVBoxLayout* contentLayout;

    QLabel* imageLabel;
    QLineEdit* imagePath;
    QPushButton* selectImage;
    QPushButton* selectFolder;
    QVBoxLayout* imageLayout;
    QHBoxLayout* buttonsLayout;

    QLabel* paramsLabel;

    QLabel* reviewThreshold;
    QDoubleSpinBox* reviewThresholdValue;
    QSlider* reviewThresholdSlider;

    QLabel* ngThreshold;
    QDoubleSpinBox* ngThresholdValue;
    QSlider* ngThresholdSlider;

    QLabel* alpha;
    QDoubleSpinBox* alphaValue;
    QSlider* alphaSlider;
    QLabel* deviceLabel;
    QComboBox* device;

    QVBoxLayout* paramsLayout;
    QHBoxLayout* reviewThresholdLayout;
    QHBoxLayout* ngThresholdLayout;
    QHBoxLayout* alphaLayout;

    void setupUI(QWidget* parent)
    {
        mainLayout = new QVBoxLayout(parent);
        mainLayout->setContentsMargins(0, 0, 0, 0);
        mainLayout->setSpacing(0);

        title = new QLabel("Inspection");
        title->setObjectName("MetricLabel");
        title->setFixedHeight(28);

        contentWidget = new QWidget(parent);
        contentWidget->setObjectName("PanelContent");
        contentLayout = new QVBoxLayout(contentWidget);
        contentLayout->setContentsMargins(8, 8, 8, 8);
        contentLayout->setSpacing(10);

        imageLabel = new QLabel("Image Source");
        imageLabel->setObjectName("SectionLabel");
        imagePath = new QLineEdit();
        selectImage = new QPushButton("Select Image");
        selectImage->setIcon(QIcon(":/file-image.png"));
        selectFolder = new QPushButton("Select Folder");
        selectFolder->setIcon(QIcon(":/folder-open.png"));
        imageLayout = new QVBoxLayout();
        imageLayout->setContentsMargins(0, 0, 0, 0);
        imageLayout->setSpacing(8);
        buttonsLayout = new QHBoxLayout();
        buttonsLayout->setContentsMargins(0, 0, 0, 0);
        buttonsLayout->setSpacing(8);

        buttonsLayout->addWidget(selectImage);
        buttonsLayout->addWidget(selectFolder);
        imageLayout->addWidget(imageLabel);
        imageLayout->addWidget(imagePath);
        imageLayout->addLayout(buttonsLayout);

        paramsLabel = new QLabel("Parameters");
        paramsLabel->setObjectName("SectionLabel");
        reviewThreshold = new QLabel("Review Threshold");
        reviewThresholdValue = new QDoubleSpinBox();
        reviewThresholdSlider = new QSlider(Qt::Horizontal);

        ngThreshold = new QLabel("NG Threshold");
        ngThresholdValue = new QDoubleSpinBox();
        ngThresholdSlider = new QSlider(Qt::Horizontal);

        alpha = new QLabel("Heatmap Alpha");
        alphaValue = new QDoubleSpinBox();
        alphaSlider = new QSlider(Qt::Horizontal);

        reviewThresholdValue->setRange(0.00, 0.99);
        reviewThresholdValue->setDecimals(2);
        reviewThresholdValue->setSingleStep(0.01);
        reviewThresholdValue->setValue(0.45);
        reviewThresholdSlider->setRange(0, 99);
        reviewThresholdSlider->setValue(45);

        ngThresholdValue->setRange(0.01, 1.00);
        ngThresholdValue->setDecimals(2);
        ngThresholdValue->setSingleStep(0.01);
        ngThresholdValue->setValue(0.60);
        ngThresholdSlider->setRange(1, 100);
        ngThresholdSlider->setValue(60);

        alphaValue->setRange(0.00, 1.00);
        alphaValue->setDecimals(2);
        alphaValue->setSingleStep(0.01);
        alphaValue->setValue(0.55);
        alphaSlider->setRange(0, 100);
        alphaSlider->setValue(55);

        deviceLabel = new QLabel("Device");
        device = new QComboBox();
        device->addItem("CPU");
        paramsLayout = new QVBoxLayout();
        paramsLayout->setContentsMargins(0, 0, 0, 0);
        paramsLayout->setSpacing(8);
        reviewThresholdLayout = new QHBoxLayout();
        reviewThresholdLayout->setContentsMargins(0, 0, 0, 0);
        reviewThresholdLayout->setSpacing(8);
        ngThresholdLayout = new QHBoxLayout();
        ngThresholdLayout->setContentsMargins(0, 0, 0, 0);
        ngThresholdLayout->setSpacing(8);
        alphaLayout = new QHBoxLayout();
        alphaLayout->setContentsMargins(0, 0, 0, 0);
        alphaLayout->setSpacing(8);

        reviewThresholdLayout->addWidget(reviewThreshold);
        reviewThresholdLayout->addWidget(reviewThresholdValue);
        ngThresholdLayout->addWidget(ngThreshold);
        ngThresholdLayout->addWidget(ngThresholdValue);
        alphaLayout->addWidget(alpha);
        alphaLayout->addWidget(alphaValue);
        paramsLayout->addWidget(paramsLabel);
        paramsLayout->addLayout(reviewThresholdLayout);
        paramsLayout->addWidget(reviewThresholdSlider);
        paramsLayout->addLayout(ngThresholdLayout);
        paramsLayout->addWidget(ngThresholdSlider);
        paramsLayout->addLayout(alphaLayout);
        paramsLayout->addWidget(alphaSlider);
        paramsLayout->addWidget(deviceLabel);
        paramsLayout->addWidget(device);
        reviewThresholdValue->setButtonSymbols(QAbstractSpinBox::NoButtons);
        ngThresholdValue->setButtonSymbols(QAbstractSpinBox::NoButtons);
        alphaValue->setButtonSymbols(QAbstractSpinBox::NoButtons);

        QFrame* separator = new QFrame();
        separator->setFrameShape(QFrame::HLine);
        separator->setObjectName("PanelSeparator");

        contentLayout->addLayout(imageLayout);
        contentLayout->addWidget(separator);
        contentLayout->addLayout(paramsLayout);

        mainLayout->addWidget(title);
        mainLayout->addWidget(contentWidget);
    }
};
