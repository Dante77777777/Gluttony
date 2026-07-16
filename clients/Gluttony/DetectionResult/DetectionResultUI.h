#include <QLabel>
#include <QTableView>
#include <QWidget>
#include <QBoxLayout>


class DetectionResultUI
{
public:
    QVBoxLayout* mainLayout;
    QLabel* titleLabel;
    QTableView* tableView;

    void setup(QWidget* parent)
    {
        mainLayout = new QVBoxLayout(parent);
        mainLayout->setSpacing(0);
        mainLayout->setContentsMargins(0,0,0,0);

        titleLabel = new QLabel("Detection Results",parent);
        titleLabel->setObjectName("MetricLabel");
        titleLabel->setFixedHeight(28);

        tableView = new QTableView(parent);
        tableView->setObjectName("DetectionResultTable");

        mainLayout->addWidget(titleLabel);
        mainLayout->addWidget(tableView);
    }

};
