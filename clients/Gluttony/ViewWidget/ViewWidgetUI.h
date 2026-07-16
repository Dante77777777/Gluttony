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
    QLabel* resultBLabel;

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
        resultBLabel = new QLabel(resultWidget);
        resultBLabel->setObjectName("ImagePanelFooter");
        resultBLabel->setFixedHeight(25);
        resultBLabel->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);

        originalLayout->addWidget(originalLabel);
        originalLayout->addWidget(originalGV);
        originalLayout->addWidget(originalBLabel);

        resultLayout->addWidget(resultLabel);
        resultLayout->addWidget(resultGV);
        resultLayout->addWidget(resultBLabel);

        parent->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);
        originalWidget->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);
        resultWidget->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);


    }
};
