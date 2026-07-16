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
    QLabel* threshold;
    QDoubleSpinBox* thresholdValue;
    QSlider* thresholdSlider;
    QLabel* alpha;
    QDoubleSpinBox* alphaValue;
    QSlider* alphaSlider;
    QLabel* deviceLabel;
    QComboBox* device;
    QVBoxLayout* paramsLayout;
    QHBoxLayout* thresholdLayout;
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
        threshold = new QLabel("Threshold");
        thresholdValue = new QDoubleSpinBox();
        thresholdSlider = new QSlider();
        thresholdSlider->setOrientation(Qt::Horizontal);
        alpha = new QLabel("Heatmap Alpha");
        alphaValue = new QDoubleSpinBox();
        alphaSlider = new QSlider();
        alphaSlider->setOrientation(Qt::Horizontal);
        deviceLabel = new QLabel("Device");
        device = new QComboBox();
        device->addItem("CPU");
        paramsLayout = new QVBoxLayout();
        paramsLayout->setContentsMargins(0, 0, 0, 0);
        paramsLayout->setSpacing(8);
        thresholdLayout = new QHBoxLayout();
        thresholdLayout->setContentsMargins(0, 0, 0, 0);
        thresholdLayout->setSpacing(8);
        alphaLayout = new QHBoxLayout();
        alphaLayout->setContentsMargins(0, 0, 0, 0);
        alphaLayout->setSpacing(8);

        thresholdLayout->addWidget(threshold);
        thresholdLayout->addWidget(thresholdValue);
        alphaLayout->addWidget(alpha);
        alphaLayout->addWidget(alphaValue);
        paramsLayout->addWidget(paramsLabel);
        paramsLayout->addLayout(thresholdLayout);
        paramsLayout->addWidget(thresholdSlider);
        paramsLayout->addLayout(alphaLayout);
        paramsLayout->addWidget(alphaSlider);
        paramsLayout->addWidget(deviceLabel);
        paramsLayout->addWidget(device);
        thresholdValue->setButtonSymbols(QAbstractSpinBox::NoButtons);
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
