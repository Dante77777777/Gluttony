#ifndef IMAGEWORKER_H
#define IMAGEWORKER_H

#include <QObject>

#include "../DataBase/PersistenceRecords.h"
#include "../Inference/InferenceEngine.h"

class ImageWorker : public QObject
{
    Q_OBJECT

public:
    explicit ImageWorker(
        QString modelPath,
        InferenceOptions options,
        QObject* parent = nullptr);

public slots:
    void initialize();
    void processItem(const ClaimedInspectionItem& item);

signals:
    void sigReady();
    void sigInitializationFailed(const QString& errorMessage);
    void sigTaskFinished(
        const ClaimedInspectionItem& item,
        const InferenceResult& result);
    void sigTaskFailed(
        const ClaimedInspectionItem& item,
        const QString& errorMessage);

private:
    QString modelPath_;
    InferenceEngine inferenceEngine_;
    InferenceOptions options_;
    bool ready_ = false;
};





#endif
