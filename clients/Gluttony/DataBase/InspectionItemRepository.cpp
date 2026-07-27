#include "InspectionItemRepository.h"

#include "SqliteTransaction.h"

#include <QMetaType>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace InspectionItemRepositoryUtils
{

void setErrorMessage(QString* errorMessage, const QString& message)
{
    if (errorMessage != nullptr)
    {
        *errorMessage = message;
    }
}

QVariant nullableString(const std::optional<QString>& value)
{
    if (value.has_value())
    {
        return value.value();
    }

    return QVariant(QMetaType::fromType<QString>());
}

bool failAndRollback(
    SqliteImmediateTransaction& transaction,
    const QString& failure,
    QString* errorMessage)
{
    QString rollbackError;
    QString finalMessage = failure;

    if (!transaction.rollback(&rollbackError))
    {
        finalMessage += QStringLiteral(" Rollback also Failed: %1")
                            .arg(rollbackError);
    }

    setErrorMessage(errorMessage, finalMessage);
    return false;
}

bool validateRunAndTime(
    const QSqlDatabase& database,
    const QString& runId,
    qint64 timestamp,
    QString* errorMessage)
{
    if (!SqliteRepositoryUtils::validateOpenSqliteDatabase(
            database,
            errorMessage))
    {
        return false;
    }

    if (runId.size() != 36 || timestamp <= 0)
    {
        setErrorMessage(
            errorMessage,
            QStringLiteral("Run ID or Timestamp is Invalid."));
        return false;
    }

    return true;
}

}

std::optional<ClaimedInspectionItem>
InspectionItemRepository::claimNextPending(
    QSqlDatabase& database,
    const QString& runId,
    qint64 claimedAtMs,
    QString* errorMessage)
{
    if (errorMessage != nullptr)
    {
        errorMessage->clear();
    }

    if (!InspectionItemRepositoryUtils::validateRunAndTime(
            database,
            runId,
            claimedAtMs,
            errorMessage))
    {
        return std::nullopt;
    }

    SqliteImmediateTransaction transaction(database);
    QString transactionError;
    if (!transaction.begin(&transactionError))
    {
        InspectionItemRepositoryUtils::setErrorMessage(
            errorMessage,
            transactionError);
        return std::nullopt;
    }

    ClaimedInspectionItem item;
    item.runId = runId;

    {
        QSqlQuery query(database);
        query.prepare(
            QStringLiteral(
                "SELECT "
                "result_id, sequence_no, source_file_name, source_file_path, "
                "source_file_size, source_modified_at_ms, source_file_hash, "
                "attempt_count "
                "FROM inspection_item "
                "WHERE run_id = :run_id AND status = 'PENDING' "
                "ORDER BY sequence_no ASC "
                "LIMIT 1;"));
        query.bindValue(QStringLiteral(":run_id"), runId);

        if (!query.exec())
        {
            InspectionItemRepositoryUtils::failAndRollback(
                transaction,
                QStringLiteral("Failed to Select Next Pending Item: %1")
                    .arg(SqliteRepositoryUtils::formatSqlError(
                        query.lastError())),
                errorMessage);
            return std::nullopt;
        }

        if (!query.next())
        {
            query.finish();
            if (!transaction.commit(&transactionError))
            {
                InspectionItemRepositoryUtils::failAndRollback(
                    transaction,
                    QStringLiteral("Failed to Commit Empty Claim: %1")
                        .arg(transactionError),
                    errorMessage);
                return std::nullopt;
            }
            return item;
        }

        item.found = true;
        item.resultId = query.value(0).toString();
        item.runId = runId;
        item.sequenceNo = query.value(1).toInt();
        item.sourceFileName = query.value(2).toString();
        item.sourceFilePath = query.value(3).toString();
        item.sourceFileSize = query.value(4).toLongLong();
        item.sourceModifiedAtMs = query.value(5).toLongLong();
        if (!query.value(6).isNull())
        {
            item.sourceFileHash = query.value(6).toString();
        }
        item.attemptCount = query.value(7).toInt() + 1;
        item.processingStartedAtMs = claimedAtMs;
        query.finish();
    }

    {
        QSqlQuery query(database);
        query.prepare(
            QStringLiteral(
                "UPDATE inspection_item "
                "SET status = 'PROCESSING', "
                "attempt_count = attempt_count + 1, "
                "processing_started_at_ms = :claimed_at_ms, "
                "updated_at_ms = :claimed_at_ms "
                "WHERE result_id = :result_id "
                "AND run_id = :run_id "
                "AND status = 'PENDING';"));
        query.bindValue(QStringLiteral(":claimed_at_ms"), claimedAtMs);
        query.bindValue(QStringLiteral(":result_id"), item.resultId);
        query.bindValue(QStringLiteral(":run_id"), runId);

        if (!query.exec())
        {
            InspectionItemRepositoryUtils::failAndRollback(
                transaction,
                QStringLiteral("Failed to Claim Pending Item: %1")
                    .arg(SqliteRepositoryUtils::formatSqlError(
                        query.lastError())),
                errorMessage);
            return std::nullopt;
        }

        const qint64 affectedRows = query.numRowsAffected();
        query.finish();
        if (affectedRows != 1)
        {
            InspectionItemRepositoryUtils::failAndRollback(
                transaction,
                QStringLiteral(
                    "Claiming Pending Item Affected %1 Rows.")
                    .arg(affectedRows),
                errorMessage);
            return std::nullopt;
        }
    }

    if (!transaction.commit(&transactionError))
    {
        InspectionItemRepositoryUtils::failAndRollback(
            transaction,
            QStringLiteral("Failed to Commit Pending Item Claim: %1")
                .arg(transactionError),
            errorMessage);
        return std::nullopt;
    }

    return item;
}

bool InspectionItemRepository::completeSuccess(
    QSqlDatabase& database,
    const SuccessfulInspectionResult& result,
    QString* errorMessage)
{
    if (errorMessage != nullptr)
    {
        errorMessage->clear();
    }

    const bool validLevel =
        result.detectionLevel == QStringLiteral("OK")
        || result.detectionLevel == QStringLiteral("REVIEW")
        || result.detectionLevel == QStringLiteral("NG");

    if (!InspectionItemRepositoryUtils::validateRunAndTime(
            database,
            result.runId,
            result.completedAtMs,
            errorMessage)
        || result.resultId.size() != 36
        || !validLevel
        || result.inferenceMs < 0
        || result.defectArea < 0
        || result.heatmapRelativePath.trimmed().isEmpty())
    {
        if (errorMessage != nullptr && errorMessage->isEmpty())
        {
            *errorMessage = QStringLiteral(
                "Successful Inspection Result is Invalid.");
        }
        return false;
    }

    SqliteImmediateTransaction transaction(database);
    QString transactionError;
    if (!transaction.begin(&transactionError))
    {
        InspectionItemRepositoryUtils::setErrorMessage(
            errorMessage,
            transactionError);
        return false;
    }

    {
        QSqlQuery query(database);
        query.prepare(
            QStringLiteral(
                "UPDATE inspection_item "
                "SET status = 'SUCCESS', "
                "detection_level = :detection_level, "
                "score = :score, "
                "inference_ms = :inference_ms, "
                "defect_area = :defect_area, "
                "heatmap_rel_path = :heatmap_rel_path, "
                "thumbnail_rel_path = :thumbnail_rel_path, "
                "completed_at_ms = :completed_at_ms, "
                "updated_at_ms = :completed_at_ms "
                "WHERE result_id = :result_id "
                "AND run_id = :run_id "
                "AND status = 'PROCESSING';"));
        query.bindValue(
            QStringLiteral(":detection_level"),
            result.detectionLevel);
        query.bindValue(QStringLiteral(":score"), result.score);
        query.bindValue(QStringLiteral(":inference_ms"), result.inferenceMs);
        query.bindValue(QStringLiteral(":defect_area"), result.defectArea);
        query.bindValue(
            QStringLiteral(":heatmap_rel_path"),
            result.heatmapRelativePath);
        query.bindValue(
            QStringLiteral(":thumbnail_rel_path"),
            InspectionItemRepositoryUtils::nullableString(
                result.thumbnailRelativePath));
        query.bindValue(
            QStringLiteral(":completed_at_ms"),
            result.completedAtMs);
        query.bindValue(QStringLiteral(":result_id"), result.resultId);
        query.bindValue(QStringLiteral(":run_id"), result.runId);

        if (!query.exec())
        {
            return InspectionItemRepositoryUtils::failAndRollback(
                transaction,
                QStringLiteral("Failed to Persist Successful Result: %1")
                    .arg(SqliteRepositoryUtils::formatSqlError(
                        query.lastError())),
                errorMessage);
        }

        const qint64 affectedRows = query.numRowsAffected();
        query.finish();
        if (affectedRows != 1)
        {
            return InspectionItemRepositoryUtils::failAndRollback(
                transaction,
                QStringLiteral(
                    "Successful Result Update Affected %1 Rows.")
                    .arg(affectedRows),
                errorMessage);
        }
    }

    {
        QSqlQuery query(database);
        query.prepare(
            QStringLiteral(
                "UPDATE inspection_run "
                "SET completed_count = completed_count + 1, "
                "ok_count = ok_count + "
                "  CASE WHEN :detection_level = 'OK' THEN 1 ELSE 0 END, "
                "review_count = review_count + "
                "  CASE WHEN :detection_level = 'REVIEW' THEN 1 ELSE 0 END, "
                "ng_count = ng_count + "
                "  CASE WHEN :detection_level = 'NG' THEN 1 ELSE 0 END, "
                "score_sum = score_sum + :score, "
                "updated_at_ms = :completed_at_ms, "
                "revision = revision + 1 "
                "WHERE run_id = :run_id AND status = 'RUNNING';"));
        query.bindValue(
            QStringLiteral(":detection_level"),
            result.detectionLevel);
        query.bindValue(QStringLiteral(":score"), result.score);
        query.bindValue(
            QStringLiteral(":completed_at_ms"),
            result.completedAtMs);
        query.bindValue(QStringLiteral(":run_id"), result.runId);

        if (!query.exec())
        {
            return InspectionItemRepositoryUtils::failAndRollback(
                transaction,
                QStringLiteral("Failed to Update Run Success Statistics: %1")
                    .arg(SqliteRepositoryUtils::formatSqlError(
                        query.lastError())),
                errorMessage);
        }

        const qint64 affectedRows = query.numRowsAffected();
        query.finish();
        if (affectedRows != 1)
        {
            return InspectionItemRepositoryUtils::failAndRollback(
                transaction,
                QStringLiteral(
                    "Run Success Statistics Update Affected %1 Rows.")
                    .arg(affectedRows),
                errorMessage);
        }
    }

    if (!transaction.commit(&transactionError))
    {
        return InspectionItemRepositoryUtils::failAndRollback(
            transaction,
            QStringLiteral("Failed to Commit Successful Result: %1")
                .arg(transactionError),
            errorMessage);
    }

    return true;
}

bool InspectionItemRepository::completeFailure(
    QSqlDatabase& database,
    const FailedInspectionResult& result,
    QString* errorMessage)
{
    if (errorMessage != nullptr)
    {
        errorMessage->clear();
    }

    if (!InspectionItemRepositoryUtils::validateRunAndTime(
            database,
            result.runId,
            result.completedAtMs,
            errorMessage)
        || result.resultId.size() != 36
        || result.errorMessage.trimmed().isEmpty())
    {
        if (errorMessage != nullptr && errorMessage->isEmpty())
        {
            *errorMessage = QStringLiteral(
                "Failed Inspection Result is Invalid.");
        }
        return false;
    }

    SqliteImmediateTransaction transaction(database);
    QString transactionError;
    if (!transaction.begin(&transactionError))
    {
        InspectionItemRepositoryUtils::setErrorMessage(
            errorMessage,
            transactionError);
        return false;
    }

    {
        QSqlQuery query(database);
        query.prepare(
            QStringLiteral(
                "UPDATE inspection_item "
                "SET status = 'FAILED', "
                "error_message = :error_message, "
                "completed_at_ms = :completed_at_ms, "
                "updated_at_ms = :completed_at_ms "
                "WHERE result_id = :result_id "
                "AND run_id = :run_id "
                "AND status = 'PROCESSING';"));
        query.bindValue(
            QStringLiteral(":error_message"),
            result.errorMessage.trimmed());
        query.bindValue(
            QStringLiteral(":completed_at_ms"),
            result.completedAtMs);
        query.bindValue(QStringLiteral(":result_id"), result.resultId);
        query.bindValue(QStringLiteral(":run_id"), result.runId);

        if (!query.exec())
        {
            return InspectionItemRepositoryUtils::failAndRollback(
                transaction,
                QStringLiteral("Failed to Persist Failed Result: %1")
                    .arg(SqliteRepositoryUtils::formatSqlError(
                        query.lastError())),
                errorMessage);
        }

        const qint64 affectedRows = query.numRowsAffected();
        query.finish();
        if (affectedRows != 1)
        {
            return InspectionItemRepositoryUtils::failAndRollback(
                transaction,
                QStringLiteral("Failed Result Update Affected %1 Rows.")
                    .arg(affectedRows),
                errorMessage);
        }
    }

    {
        QSqlQuery query(database);
        query.prepare(
            QStringLiteral(
                "UPDATE inspection_run "
                "SET completed_count = completed_count + 1, "
                "failed_count = failed_count + 1, "
                "updated_at_ms = :completed_at_ms, "
                "revision = revision + 1 "
                "WHERE run_id = :run_id AND status = 'RUNNING';"));
        query.bindValue(
            QStringLiteral(":completed_at_ms"),
            result.completedAtMs);
        query.bindValue(QStringLiteral(":run_id"), result.runId);

        if (!query.exec())
        {
            return InspectionItemRepositoryUtils::failAndRollback(
                transaction,
                QStringLiteral("Failed to Update Run Failure Statistics: %1")
                    .arg(SqliteRepositoryUtils::formatSqlError(
                        query.lastError())),
                errorMessage);
        }

        const qint64 affectedRows = query.numRowsAffected();
        query.finish();
        if (affectedRows != 1)
        {
            return InspectionItemRepositoryUtils::failAndRollback(
                transaction,
                QStringLiteral(
                    "Run Failure Statistics Update Affected %1 Rows.")
                    .arg(affectedRows),
                errorMessage);
        }
    }

    if (!transaction.commit(&transactionError))
    {
        return InspectionItemRepositoryUtils::failAndRollback(
            transaction,
            QStringLiteral("Failed to Commit Failed Result: %1")
                .arg(transactionError),
            errorMessage);
    }

    return true;
}

std::optional<QVector<InspectionItemPageRow>>
InspectionItemRepository::listPage(
    const QSqlDatabase& database,
    const QString& runId,
    int lastSequenceNo,
    int pageSize,
    QString* errorMessage)
{
    if (errorMessage != nullptr)
    {
        errorMessage->clear();
    }

    if (!SqliteRepositoryUtils::validateOpenSqliteDatabase(
            database,
            errorMessage)
        || runId.size() != 36
        || lastSequenceNo < 0
        || pageSize < 1
        || pageSize > 1000)
    {
        if (errorMessage != nullptr && errorMessage->isEmpty())
        {
            *errorMessage = QStringLiteral("Item Page Request is Invalid.");
        }
        return std::nullopt;
    }

    QSqlQuery query(database);
    query.prepare(
        QStringLiteral(
            "SELECT "
            "result_id, sequence_no, source_file_name, source_file_path, "
            "status, detection_level, score, inference_ms, defect_area, "
            "heatmap_rel_path, thumbnail_rel_path, error_message "
            "FROM inspection_item "
            "WHERE run_id = :run_id AND sequence_no > :last_sequence_no "
            "ORDER BY sequence_no ASC "
            "LIMIT :page_size;"));
    query.bindValue(QStringLiteral(":run_id"), runId);
    query.bindValue(QStringLiteral(":last_sequence_no"), lastSequenceNo);
    query.bindValue(QStringLiteral(":page_size"), pageSize);

    if (!query.exec())
    {
        InspectionItemRepositoryUtils::setErrorMessage(
            errorMessage,
            QStringLiteral("Failed to Query Inspection Item Page: %1")
                .arg(SqliteRepositoryUtils::formatSqlError(
                    query.lastError())));
        return std::nullopt;
    }

    QVector<InspectionItemPageRow> rows;
    while (query.next())
    {
        InspectionItemPageRow row;
        row.resultId = query.value(0).toString();
        row.sequenceNo = query.value(1).toInt();
        row.sourceFileName = query.value(2).toString();
        row.sourceFilePath = query.value(3).toString();
        row.status = query.value(4).toString();
        if (!query.value(5).isNull())
        {
            row.detectionLevel = query.value(5).toString();
        }
        if (!query.value(6).isNull())
        {
            row.score = query.value(6).toDouble();
        }
        if (!query.value(7).isNull())
        {
            row.inferenceMs = query.value(7).toLongLong();
        }
        if (!query.value(8).isNull())
        {
            row.defectArea = query.value(8).toLongLong();
        }
        if (!query.value(9).isNull())
        {
            row.heatmapRelativePath = query.value(9).toString();
        }
        if (!query.value(10).isNull())
        {
            row.thumbnailRelativePath = query.value(10).toString();
        }
        if (!query.value(11).isNull())
        {
            row.errorMessage = query.value(11).toString();
        }
        rows.push_back(std::move(row));
    }

    if (query.lastError().isValid())
    {
        InspectionItemRepositoryUtils::setErrorMessage(
            errorMessage,
            QStringLiteral("Failed While Reading Inspection Item Page: %1")
                .arg(SqliteRepositoryUtils::formatSqlError(
                    query.lastError())));
        return std::nullopt;
    }

    query.finish();
    return rows;
}
