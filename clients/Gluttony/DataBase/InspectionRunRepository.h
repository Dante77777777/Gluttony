#ifndef INSPECTIONRUNREPOSITORY_H
#define INSPECTIONRUNREPOSITORY_H

#include "PersistenceRecords.h"

#include <QSqlDatabase>
#include <QString>
#include <QVector>

#include <optional>

class InspectionRunRepository final
{
public:
    [[nodiscard]]
    static bool createWithInputItems(
        QSqlDatabase& database,
        const NewInspectionRun& run,
        const QVector<NewInspectionItem>& items,
        QString* errorMessage = nullptr);

    [[nodiscard]]
    static std::optional<RecoveryReport> recoverInterruptedRuns(
        QSqlDatabase& database,
        qint64 recoveredAtMs,
        QString* errorMessage = nullptr);

    [[nodiscard]]
    static bool resumeInterruptedRun(
        QSqlDatabase& database,
        const QString& runId,
        qint64 resumedAtMs,
        QString* errorMessage = nullptr);

    [[nodiscard]]
    static bool finishAsDraft(
        QSqlDatabase& database,
        const QString& runId,
        qint64 completedAtMs,
        QString* errorMessage = nullptr);

    [[nodiscard]]
    static bool saveDraft(
        QSqlDatabase& database,
        const SaveInspectionRunRequest& request,
        QString* errorMessage = nullptr);

    [[nodiscard]]
    static bool discardDraft(
        QSqlDatabase& database,
        const QString& runId,
        qint64 createdAtMs,
        QString* errorMessage = nullptr);

    [[nodiscard]]
    static bool deleteSaved(
        QSqlDatabase& database,
        const QString& runId,
        qint64 createdAtMs,
        QString* errorMessage = nullptr);

    [[nodiscard]]
    static std::optional<QVector<InspectionRunHistoryRow>> listSavedHistory(
        const QSqlDatabase& database,
        int pageSize,
        std::optional<qint64> beforeSavedAtMs = std::nullopt,
        const QString& beforeRunId = QString(),
        QString* errorMessage = nullptr);
};

#endif
