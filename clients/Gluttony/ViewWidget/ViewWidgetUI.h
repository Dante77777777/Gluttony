#include <QLabel>
#include <QBoxLayout>
#include <QPushButton>
#include <QComboBox>
#include <QWidget>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QSlider>
#include <QFrame>
#include "GraphicsView.h"

class ViewWidgetUI
{
public:
    QHBoxLayout* mainLayout;

    QWidget* originalWidget;
    QVBoxLayout* originalLayout;
    QLabel* originalLabel;
    GraphicsView* originalGV;
    QLabel* originalBLabel;

    QWidget* resultWidget;
    QVBoxLayout* resultLayout;
    QLabel* resultLabel;
    GraphicsView* resultGV;
    QWidget* resultFooter;
    QLabel* heatmapLabel;
    QLabel* heatmapLowLabel;
    QFrame* heatmapGradient;
    QLabel* heatmapHighLabel;

    void setupUI(QWidget* parent)
    {
        mainLayout = new QHBoxLayout(parent);
        mainLayout->setContentsMargins(0, 0, 0, 0);
        mainLayout->setSpacing(12);

        parent->setObjectName("ViewWidget");
        originalWidget = new QWidget(parent);
        originalWidget->setObjectName("ImagePanel");
        resultWidget = new QWidget(parent);
        resultWidget->setObjectName("ImagePanel");
        mainLayout->addWidget(originalWidget);
        mainLayout->addWidget(resultWidget);

        originalLayout = new QVBoxLayout(originalWidget);
        resultLayout = new QVBoxLayout(resultWidget);
        originalLayout->setContentsMargins(10, 10, 10, 10);
        resultLayout->setContentsMargins(10, 10, 10, 10);
        originalLayout->setSpacing(8);
        resultLayout->setSpacing(8);

        originalLabel = new QLabel("Original");
        originalLabel->setObjectName("ImagePanelTitle");
        originalLabel->setFixedHeight(24);
        originalLabel->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
        resultLabel = new QLabel("Heatmap Overlay");
        resultLabel->setObjectName("ImagePanelTitle");
        resultLabel->setFixedHeight(24);
        resultLabel->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);

        originalGV = new GraphicsView(originalWidget);
        originalGV->setObjectName("ImageGraphicsView");
        resultGV = new GraphicsView(resultWidget);
        resultGV->setObjectName("ImageGraphicsView");

        originalBLabel = new QLabel(originalWidget);
        originalBLabel->setObjectName("ImagePanelFooter");
        originalBLabel->setFixedHeight(25);
        originalBLabel->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
        originalBLabel->setText("No image selected");

        resultFooter = new QWidget(resultWidget);
        resultFooter->setObjectName("ResultImageFooter");
        resultFooter->setFixedHeight(25);
        resultFooter->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);

        auto* resultFooterLayout = new QHBoxLayout(resultFooter);
        resultFooterLayout->setContentsMargins(8, 0, 8, 0);
        resultFooterLayout->setSpacing(8);

        heatmapLabel = new QLabel("Heatmap", resultFooter);
        heatmapLabel->setObjectName("HeatmapLegendLabel");
        heatmapLowLabel = new QLabel("Low", resultFooter);
        heatmapLowLabel->setObjectName("HeatmapLegendLabel");

        heatmapGradient = new QFrame(resultFooter);
        heatmapGradient->setObjectName("HeatmapGradient");
        heatmapGradient->setFixedSize(140, 14);

        heatmapHighLabel = new QLabel("High", resultFooter);
        heatmapHighLabel->setObjectName("HeatmapLegendLabel");

        resultFooterLayout->addWidget(heatmapLabel);
        resultFooterLayout->addWidget(heatmapLowLabel);
        resultFooterLayout->addWidget(heatmapGradient);
        resultFooterLayout->addWidget(heatmapHighLabel);
        resultFooterLayout->addStretch();

        originalLayout->addWidget(originalLabel);
        originalLayout->addWidget(originalGV);
        originalLayout->addWidget(originalBLabel);

        resultLayout->addWidget(resultLabel);
        resultLayout->addWidget(resultGV);
        resultLayout->addWidget(resultFooter);

        parent->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);
        originalWidget->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);
        resultWidget->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);


    }
};
