#include "DetectionResult.h"
#include <QHeaderView>

DetectionResult::DetectionResult(QWidget* parent)
    : QWidget(parent)
    , ui(new DetectionResultUI())
    , model(new DetectionResultModel())
{
    setup();
}

void DetectionResult::setup()
{
    ui->setup(this);
    ui->tableView->setModel(model);
    ui->tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableView->setAlternatingRowColors(true);

    ui->tableView->horizontalHeader()
        ->setSectionResizeMode(QHeaderView::Stretch);

    //this->setFixedHeight(240);
    //this->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);
}
