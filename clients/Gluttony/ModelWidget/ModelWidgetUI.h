#include <QLabel>
#include <QBoxLayout>
#include <QPushButton>
#include <QComboBox>
#include <QWidget>

class ModelWidgetUI
{
public:
    QLabel* title;
    QVBoxLayout* layout;
    QWidget* contentWidget;
    QVBoxLayout* contentLayout;
    QPushButton* loadModelButton;
    QComboBox* models;

    void setupUI(QWidget* parent)
    {
        layout = new QVBoxLayout(parent);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);

        title = new QLabel();
        title->setText("Model");
        title->setObjectName("MetricLabel");
        title->setFixedHeight(28);

        contentWidget = new QWidget(parent);
        contentWidget->setObjectName("PanelContent");

        contentLayout = new QVBoxLayout(contentWidget);
        contentLayout->setContentsMargins(8, 8, 8, 8);
        contentLayout->setSpacing(8);

        loadModelButton = new QPushButton();
        models = new QComboBox();
        loadModelButton->setText("Load Model");
        loadModelButton->setIcon(QIcon(":/model.png"));
        // models->addItem("1",1);
        // models->addItem("2",2);
        // models->addItem("3",3);

        contentLayout->addWidget(models);
        contentLayout->addWidget(loadModelButton);

        layout->addWidget(title);
        layout->addWidget(contentWidget);
    }
};
