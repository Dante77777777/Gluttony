#ifndef PERSISTENCERECORDS_H
#define PERSISTENCERECORDS_H

#include <QString>
#include <QVector>
#include <QMetaType>

#include <optional>

struct NewInspectionRun final
{
    QString runId;
    QString sourceType;
    QString sourcePath;
    QString storagePath;
    QString storageOrigin;
    QString modelPath;
    QString modelFormat;
    QString modelHash;
    QString appVersion;
    double reviewThreshold = 0.0;
    double ngThreshold = 0.0;
    double heatmapAlpha = 0.0;
    qint64 startedAtMs = 0;
};

struct NewInspectionItem final
{
    QString resultId;
    int sequenceNo = 0;
    QString sourceFileName;
    QString sourceFilePath;
    qint64 sourceFileSize = 0;
    qint64 sourceModifiedAtMs = 0;
    std::optional<QString> sourceFileHash;
    qint64 createdAtMs = 0;
};

struct ClaimedInspectionItem final
{
    bool found = false;
    QString resultId;
    QString runId;
    int sequenceNo = 0;
    QString sourceFileName;
    QString sourceFilePath;
    qint64 sourceFileSize = 0;
    qint64 sourceModifiedAtMs = 0;
    std::optional<QString> sourceFileHash;
    int attemptCount = 0;
    qint64 processingStartedAtMs = 0;
};

struct SuccessfulInspectionResult final
{
    QString resultId;
    QString runId;
    QString detectionLevel;
    double score = 0.0;
    qint64 inferenceMs = 0;
    qint64 defectArea = 0;
    QString heatmapRelativePath;
    std::optional<QString> thumbnailRelativePath;
    qint64 completedAtMs = 0;
};

struct FailedInspectionResult final
{
    QString resultId;
    QString runId;
    QString errorMessage;
    qint64 completedAtMs = 0;
};

struct SaveInspectionRunRequest final
{
    QString runId;
    QString name;
    std::optional<QString> note;
    qint64 savedAtMs = 0;
    int manifestVersion = 0;
    QString manifestRelativePath;
    QString manifestSha256;
    QString itemsRelativePath;
    QString itemsSha256;
};

struct InspectionRunHistoryRow final
{
    QString runId;
    QString name;
    std::optional<QString> note;
    QString sourceType;
    QString sourcePath;
    QString modelFormat;
    QString modelPath;
    qint64 startedAtMs = 0;
    qint64 completedAtMs = 0;
    qint64 savedAtMs = 0;
    qint64 totalCount = 0;
    qint64 completedCount = 0;
    qint64 failedCount = 0;
    qint64 okCount = 0;
    qint64 reviewCount = 0;
    qint64 ngCount = 0;
    std::optional<double> averageScore;
};

struct InspectionItemPageRow final
{
    QString resultId;
    int sequenceNo = 0;
    QString sourceFileName;
    QString sourceFilePath;
    QString status;
    std::optional<QString> detectionLevel;
    std::optional<double> score;
    std::optional<qint64> inferenceMs;
    std::optional<qint64> defectArea;
    std::optional<QString> heatmapRelativePath;
    std::optional<QString> thumbnailRelativePath;
    std::optional<QString> errorMessage;
};

struct InspectionRunRecord final
{
    QString runId;
    std::optional<QString> name;
    std::optional<QString> note;
    QString status;
    QString sourceType;
    QString sourcePath;
    QString storagePath;
    QString storageOrigin;
    QString modelPath;
    QString modelFormat;
    QString modelHash;
    QString appVersion;
    double reviewThreshold = 0.0;
    double ngThreshold = 0.0;
    double heatmapAlpha = 0.0;
    qint64 startedAtMs = 0;
    std::optional<qint64> completedAtMs;
    std::optional<qint64> savedAtMs;
    qint64 totalCount = 0;
    qint64 completedCount = 0;
    qint64 failedCount = 0;
    qint64 okCount = 0;
    qint64 reviewCount = 0;
    qint64 ngCount = 0;
    double scoreSum = 0.0;
};

struct PersistedInspectionResult final
{
    QString resultId;
    QString runId;
    int sequenceNo = 0;
    QString sourceFileName;
    QString sourceFilePath;
    QString detectionLevel;
    double score = 0.0;
    qint64 inferenceMs = 0;
    qint64 defectArea = 0;
    QString heatmapFilePath;
};

struct RecoveryReport final
{
    qint64 resetItemCount = 0;
    qint64 interruptedRunCount = 0;
};

using InspectionItemRows = QVector<InspectionItemPageRow>;
using InspectionRunRecords = QVector<InspectionRunRecord>;
using InspectionRunHistoryRows = QVector<InspectionRunHistoryRow>;

Q_DECLARE_METATYPE(ClaimedInspectionItem)
Q_DECLARE_METATYPE(InspectionItemPageRow)
Q_DECLARE_METATYPE(InspectionItemRows)
Q_DECLARE_METATYPE(InspectionRunRecord)
Q_DECLARE_METATYPE(InspectionRunRecords)
Q_DECLARE_METATYPE(InspectionRunHistoryRows)
Q_DECLARE_METATYPE(PersistedInspectionResult)

#endif
