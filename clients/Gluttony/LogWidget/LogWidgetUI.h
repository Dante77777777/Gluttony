#include <QWidget>
#include <QBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTextBrowser>
#include <QIcon>


class LogWidgetUI
{
public:
    QVBoxLayout* mainLayout;
    QWidget* titleWidget;
    QHBoxLayout* titleLayout;
    QLabel* title;
    QPushButton* saveButton;
    QPushButton* clearButton;
    QTextBrowser* logs;

    void setupUI(QWidget* parent)
    {
        parent->setObjectName("LogWidget");
        mainLayout = new QVBoxLayout(parent);
        mainLayout->setContentsMargins(12, 12, 12, 12);
        mainLayout->setSpacing(8);
        titleWidget = new QWidget();
        titleWidget->setObjectName("LogTitleBar");
        titleLayout = new QHBoxLayout(titleWidget);
        titleLayout->setContentsMargins(0, 0, 0, 0);
        titleLayout->setSpacing(8);

        title = new QLabel("Log");
        title->setObjectName("LogTitle");
        clearButton = new QPushButton("Clear");
        clearButton->setObjectName("LogClearButton");
        saveButton = new QPushButton();
        saveButton->setObjectName("LogSaveButton");
        saveButton->setIcon(QIcon(":/save.png"));

        titleLayout->addWidget(title);
        titleLayout->addStretch();
        titleLayout->addWidget(clearButton);
        titleLayout->addWidget(saveButton);

        logs = new QTextBrowser();
        logs->setObjectName("LogPanel");
        logs->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);

        mainLayout->addWidget(titleWidget);
        mainLayout->addWidget(logs);
    }
};
