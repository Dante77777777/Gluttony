#ifndef DETECTIONRESULTMODEL_H
#define DETECTIONRESULTMODEL_H

#include <QAbstractTableModel>
#include <QVector>
#include <QString>
#include <QColor>
#include <QImage>
#include <QBrush>

struct DetectionRecord
{
    QString fileName;
    QString sourceFileName;
    bool isNG;
    double score;
    int inferenceMS;
    int defectArea;
    QImage heatmapOverlay;
};

class DetectionResultModel : public QAbstractTableModel
{
public:
    explicit DetectionResultModel(QObject* parent = nullptr);
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    void appendResult(const DetectionRecord& record);
    void clear();

    bool recordAt(int row, DetectionRecord& record) const;

private:
    QVector<DetectionRecord> records;
};









#endif
