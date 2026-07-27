#ifndef DETECTIONRESULTMODEL_H
#define DETECTIONRESULTMODEL_H

#include <QAbstractTableModel>
#include <QVector>
#include <QString>
#include <QColor>
#include <QBrush>
#include "../Inference/InferenceEngine.h"

struct DetectionRecord
{
    QString runId;
    QString resultId;
    int sequenceNo = 0;
    QString fileName;
    QString sourceFileName;
    QString heatmapFileName;
    QString status = QStringLiteral("SUCCESS");
    DetectionLevel detectionLevel = DetectionLevel::OK;
    bool hasDetectionResult = true;
    double score = 0.0;
    int inferenceMS = 0;
    int defectArea = 0;
    QString errorMessage;
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
