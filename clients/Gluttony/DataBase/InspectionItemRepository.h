#ifndef INSPECTIONITEMREPOSITORY_H
#define INSPECTIONITEMREPOSITORY_H

#include "PersistenceRecords.h"

#include <QSqlDatabase>
#include <QString>
#include <QVector>

#include <optional>

class InspectionItemRepository final
{
public:
    [[nodiscard]]
    static std::optional<ClaimedInspectionItem> claimNextPending(
        QSqlDatabase& database,
        const QString& runId,
        qint64 claimedAtMs,
        QString* errorMessage = nullptr);

    [[nodiscard]]
    static bool completeSuccess(
        QSqlDatabase& database,
        const SuccessfulInspectionResult& result,
        QString* errorMessage = nullptr);

    [[nodiscard]]
    static bool completeFailure(
        QSqlDatabase& database,
        const FailedInspectionResult& result,
        QString* errorMessage = nullptr);

    [[nodiscard]]
    static std::optional<QVector<InspectionItemPageRow>> listPage(
        const QSqlDatabase& database,
        const QString& runId,
        int lastSequenceNo,
        int pageSize,
        QString* errorMessage = nullptr);
};

#endif
