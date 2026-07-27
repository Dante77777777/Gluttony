#include "DetectionResultModel.h"

namespace LevelConver {

QString levelText(DetectionLevel level)
{
    switch (level) {
    case DetectionLevel::OK:
        return "● OK";
        break;
    case DetectionLevel::REVIEW:
        return "● REVIEW";
        break;
    case DetectionLevel::NG:
        return "● NG";
        break;
    }
    return {};
}

QColor levelColor(DetectionLevel level)
{
    switch (level) {
    case DetectionLevel::OK:
        return QColor("#16a34a");

    case DetectionLevel::REVIEW:
        return QColor("#d97706");

    case DetectionLevel::NG:
        return QColor("#dc2626");
    }

    return QColor("#1f2937");
}

QString recordStatusText(const DetectionRecord& record)
{
    if (!record.hasDetectionResult)
    {
        return QStringLiteral("● FAILED");
    }
    return levelText(record.detectionLevel);
}

QColor recordStatusColor(const DetectionRecord& record)
{
    if (!record.hasDetectionResult)
    {
        return QColor(QStringLiteral("#6b7280"));
    }
    return levelColor(record.detectionLevel);
}

}



DetectionResultModel::DetectionResultModel(QObject* parent)
    : QAbstractTableModel(parent)
    , records(0)
{

}

int DetectionResultModel::rowCount(const QModelIndex& parent) const
{
    if(parent.isValid())
    {
        return 0;
    }
    return records.size();
}

int DetectionResultModel::columnCount(const QModelIndex& parent) const
{
    if(parent.isValid())
    {
        return 0;
    }
    return 5;
}

QVariant DetectionResultModel::data(const QModelIndex& index, int role) const
{
    if(!index.isValid() || index.row() >= records.size())
    {
        return {};
    }

    const DetectionRecord &record = records.at(index.row());
    if(role == Qt::DisplayRole)
    {
        switch (index.column()) {
        case 0:
            return record.fileName;
            break;
        case 1:
            return LevelConver::recordStatusText(record);
            break;
        case 2:
            return record.hasDetectionResult
                       ? QString::number(record.score, 'f', 3)
                       : QStringLiteral("-");
            break;
        case 3:
            return record.hasDetectionResult
                       ? QStringLiteral("%1ms").arg(record.inferenceMS)
                       : QStringLiteral("-");
            break;
        case 4:
            return record.hasDetectionResult
                       ? QStringLiteral("%1px").arg(record.defectArea)
                       : QStringLiteral("-");
            break;
        }
    }

    if(role == Qt::ForegroundRole && (index.column() == 1 || index.column() == 0))
    {
        return QBrush(LevelConver::recordStatusColor(record));
    }

    if(role == Qt::TextAlignmentRole)
    {
        if(index.column() == 0)
        {
            return QVariant::fromValue(Qt::AlignVCenter | Qt::AlignLeft);
        }
        return Qt::AlignCenter;
    }

    return {};
}

QVariant DetectionResultModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if(role != Qt::DisplayRole)
    {
        return {};
    }

    if(orientation == Qt::Vertical)
    {
        return section+1;
    }

    static const QStringList headers = {"File","Status","Score","Time","Area"};

    if(section >= 0 && section < headers.size())
    {
        return headers.at(section);
    }
    return {};
}

void DetectionResultModel::appendResult(const DetectionRecord& record)
{
    const int newRow = records.size();
    beginInsertRows(QModelIndex(),newRow,newRow);
    records.append(record);
    endInsertRows();
}

void DetectionResultModel::clear()
{
    beginResetModel();
    records.clear();
    endResetModel();
}


bool DetectionResultModel::recordAt(int row, DetectionRecord& record) const
{
    if(row < 0 || row >= records.size())
    {
        return false;
    }
    record = records.at(row);
    return true;
}
