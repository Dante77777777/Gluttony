#include "InspectionRunRepository.h"

#include "MigrationCheckSum.h"
#include "SqliteTransaction.h"

#include <QMetaType>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace InspectionRunRepositoryUtils
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

bool validateRun(const NewInspectionRun& run, QString* errorMessage)
{
    if (run.runId.size() != 36)
    {
        setErrorMessage(
            errorMessage,
            QStringLiteral("Run ID Must Contain 36 Characters."));
        return false;
    }

    if (run.sourcePath.trimmed().isEmpty()
        || run.storagePath.trimmed().isEmpty()
        || run.modelPath.trimmed().isEmpty()
        || run.appVersion.trimmed().isEmpty())
    {
        setErrorMessage(
            errorMessage,
            QStringLiteral(
                "Source, Storage, Model and Application Paths/Version "
                "Must not be Empty."));
        return false;
    }

    if (run.sourceType != QStringLiteral("SINGLE_IMAGE")
        && run.sourceType != QStringLiteral("FOLDER"))
    {
        setErrorMessage(
            errorMessage,
            QStringLiteral("Invalid Source Type: %1").arg(run.sourceType));
        return false;
    }

    if (run.storageOrigin != QStringLiteral("MANAGED")
        && run.storageOrigin != QStringLiteral("EXTERNAL"))
    {
        setErrorMessage(
            errorMessage,
            QStringLiteral("Invalid Storage Origin: %1")
                .arg(run.storageOrigin));
        return false;
    }

    if (run.modelFormat != QStringLiteral("ONNX")
        && run.modelFormat != QStringLiteral("OPENVINO"))
    {
        setErrorMessage(
            errorMessage,
            QStringLiteral("Invalid Model Format: %1")
                .arg(run.modelFormat));
        return false;
    }

    if (!MigrationCheckSum::isValidSha256Hex(run.modelHash))
    {
        setErrorMessage(
            errorMessage,
            QStringLiteral("Model Hash Must be a Lowercase SHA-256 Value."));
        return false;
    }

    if (run.reviewThreshold < 0.0
        || run.reviewThreshold > 1.0
        || run.ngThreshold < 0.0
        || run.ngThreshold > 1.0
        || run.reviewThreshold >= run.ngThreshold
        || run.heatmapAlpha < 0.0
        || run.heatmapAlpha > 1.0)
    {
        setErrorMessage(
            errorMessage,
            QStringLiteral("Detection Threshold Snapshot is Invalid."));
        return false;
    }

    if (run.startedAtMs <= 0)
    {
        setErrorMessage(
            errorMessage,
            QStringLiteral("Run Start Timestamp Must be Positive."));
        return false;
    }

    return true;
}

bool validateItems(
    const QVector<NewInspectionItem>& items,
    QString* errorMessage)
{
    if (items.isEmpty())
    {
        setErrorMessage(
            errorMessage,
            QStringLiteral("A Detection Run Must Contain at Least One Item."));
        return false;
    }

    QSet<QString> resultIds;

    for (qsizetype index = 0; index < items.size(); ++index)
    {
        const NewInspectionItem& item = items.at(index);
        const int expectedSequence = static_cast<int>(index + 1);

        if (item.resultId.size() != 36)
        {
            setErrorMessage(
                errorMessage,
                QStringLiteral("Item %1 Result ID Must Contain 36 Characters.")
                    .arg(expectedSequence));
            return false;
        }

        if (resultIds.contains(item.resultId))
        {
            setErrorMessage(
                errorMessage,
                QStringLiteral("Duplicate Result ID: %1")
                    .arg(item.resultId));
            return false;
        }

        if (item.sequenceNo != expectedSequence)
        {
            setErrorMessage(
                errorMessage,
                QStringLiteral(
                    "Input Sequence Must be Consecutive Starting at 1. "
                    "Expected=%1, Actual=%2")
                    .arg(expectedSequence)
                    .arg(item.sequenceNo));
            return false;
        }

        if (item.sourceFileName.trimmed().isEmpty()
            || item.sourceFilePath.trimmed().isEmpty()
            || item.sourceFileSize < 0
            || item.sourceModifiedAtMs < 0
            || item.createdAtMs <= 0)
        {
            setErrorMessage(
                errorMessage,
                QStringLiteral("Input Metadata is Invalid at Sequence %1.")
                    .arg(item.sequenceNo));
            return false;
        }

        if (item.sourceFileHash.has_value()
            && !MigrationCheckSum::isValidSha256Hex(
                item.sourceFileHash.value()))
        {
            setErrorMessage(
                errorMessage,
                QStringLiteral("Source File Hash is Invalid at Sequence %1.")
                    .arg(item.sequenceNo));
            return false;
        }

        resultIds.insert(item.resultId);
    }

    return true;
}

bool deleteRun(
    QSqlDatabase& database,
    const QString& runId,
    const QString& expectedStatus,
    const QString& cleanupReason,
    qint64 createdAtMs,
    QString* errorMessage)
{
    if (runId.size() != 36 || createdAtMs <= 0)
    {
        setErrorMessage(
            errorMessage,
            QStringLiteral("Run ID or Cleanup Timestamp is Invalid."));
        return false;
    }

    SqliteImmediateTransaction transaction(database);
    QString transactionError;
    if (!transaction.begin(&transactionError))
    {
        setErrorMessage(errorMessage, transactionError);
        return false;
    }

    QString storageOrigin;
    QString storagePath;
    QString status;

    {
        QSqlQuery query(database);
        query.prepare(
            QStringLiteral(
                "SELECT storage_origin, storage_path, status "
                "FROM inspection_run WHERE run_id = :run_id;"));
        query.bindValue(QStringLiteral(":run_id"), runId);

        if (!query.exec())
        {
            return failAndRollback(
                transaction,
                QStringLiteral("Failed to Read Run Before Deletion: %1")
                    .arg(SqliteRepositoryUtils::formatSqlError(
                        query.lastError())),
                errorMessage);
        }

        if (!query.next())
        {
            query.finish();
            return failAndRollback(
                transaction,
                QStringLiteral("Inspection Run Does not Exist: %1")
                    .arg(runId),
                errorMessage);
        }

        storageOrigin = query.value(0).toString();
        storagePath = query.value(1).toString();
        status = query.value(2).toString();
        query.finish();
    }

    if (status != expectedStatus)
    {
        return failAndRollback(
            transaction,
            QStringLiteral(
                "Inspection Run %1 Must be %2 Before Deletion; Actual=%3")
                .arg(runId)
                .arg(expectedStatus)
                .arg(status),
            errorMessage);
    }

    if (storageOrigin == QStringLiteral("MANAGED"))
    {
        QSqlQuery cleanupQuery(database);
        cleanupQuery.prepare(
            QStringLiteral(
                "INSERT INTO storage_cleanup_job ("
                "target_path, target_kind, reason, origin_run_id, created_at_ms"
                ") VALUES ("
                ":target_path, 'DIRECTORY', :reason, :run_id, :created_at_ms"
                ") ON CONFLICT(target_path) DO NOTHING;"));
        cleanupQuery.bindValue(QStringLiteral(":target_path"), storagePath);
        cleanupQuery.bindValue(QStringLiteral(":reason"), cleanupReason);
        cleanupQuery.bindValue(QStringLiteral(":run_id"), runId);
        cleanupQuery.bindValue(QStringLiteral(":created_at_ms"), createdAtMs);

        if (!cleanupQuery.exec())
        {
            return failAndRollback(
                transaction,
                QStringLiteral("Failed to Schedule Run Directory Cleanup: %1")
                    .arg(SqliteRepositoryUtils::formatSqlError(
                        cleanupQuery.lastError())),
                errorMessage);
        }
        cleanupQuery.finish();
    }

    QSqlQuery deleteQuery(database);
    deleteQuery.prepare(
        QStringLiteral(
            "DELETE FROM inspection_run "
            "WHERE run_id = :run_id AND status = :status;"));
    deleteQuery.bindValue(QStringLiteral(":run_id"), runId);
    deleteQuery.bindValue(QStringLiteral(":status"), expectedStatus);

    if (!deleteQuery.exec())
    {
        return failAndRollback(
            transaction,
            QStringLiteral("Failed to Delete Inspection Run: %1")
                .arg(SqliteRepositoryUtils::formatSqlError(
                    deleteQuery.lastError())),
            errorMessage);
    }

    const qint64 affectedRows = deleteQuery.numRowsAffected();
    deleteQuery.finish();

    if (affectedRows != 1)
    {
        return failAndRollback(
            transaction,
            QStringLiteral(
                "Inspection Run Delete Affected an Unexpected Number "
                "of Rows. Expected=1, Actual=%1")
                .arg(affectedRows),
            errorMessage);
    }

    if (!transaction.commit(&transactionError))
    {
        return failAndRollback(
            transaction,
            QStringLiteral("Failed to Commit Run Deletion: %1")
                .arg(transactionError),
            errorMessage);
    }

    return true;
}

}

bool InspectionRunRepository::createWithInputItems(
    QSqlDatabase& database,
    const NewInspectionRun& run,
    const QVector<NewInspectionItem>& items,
    QString* errorMessage)
{
    if (errorMessage != nullptr)
    {
        errorMessage->clear();
    }

    if (!SqliteRepositoryUtils::validateOpenSqliteDatabase(
            database,
            errorMessage)
        || !InspectionRunRepositoryUtils::validateRun(run, errorMessage)
        || !InspectionRunRepositoryUtils::validateItems(items, errorMessage))
    {
        return false;
    }

    SqliteImmediateTransaction transaction(database);
    QString transactionError;
    if (!transaction.begin(&transactionError))
    {
        InspectionRunRepositoryUtils::setErrorMessage(
            errorMessage,
            transactionError);
        return false;
    }

    {
        QSqlQuery query(database);
        query.prepare(
            QStringLiteral(
                "INSERT INTO inspection_run ("
                "run_id, status, source_type, source_path, "
                "storage_path, storage_origin, model_path, model_format, "
                "model_hash, app_version, review_threshold, ng_threshold, "
                "heatmap_alpha, started_at_ms, updated_at_ms, total_count"
                ") VALUES ("
                ":run_id, 'RUNNING', :source_type, :source_path, "
                ":storage_path, :storage_origin, :model_path, :model_format, "
                ":model_hash, :app_version, :review_threshold, :ng_threshold, "
                ":heatmap_alpha, :started_at_ms, :updated_at_ms, 0"
                ");"));
        query.bindValue(QStringLiteral(":run_id"), run.runId);
        query.bindValue(QStringLiteral(":source_type"), run.sourceType);
        query.bindValue(QStringLiteral(":source_path"), run.sourcePath);
        query.bindValue(QStringLiteral(":storage_path"), run.storagePath);
        query.bindValue(QStringLiteral(":storage_origin"), run.storageOrigin);
        query.bindValue(QStringLiteral(":model_path"), run.modelPath);
        query.bindValue(QStringLiteral(":model_format"), run.modelFormat);
        query.bindValue(QStringLiteral(":model_hash"), run.modelHash);
        query.bindValue(QStringLiteral(":app_version"), run.appVersion);
        query.bindValue(QStringLiteral(":review_threshold"), run.reviewThreshold);
        query.bindValue(QStringLiteral(":ng_threshold"), run.ngThreshold);
        query.bindValue(QStringLiteral(":heatmap_alpha"), run.heatmapAlpha);
        query.bindValue(QStringLiteral(":started_at_ms"), run.startedAtMs);
        query.bindValue(QStringLiteral(":updated_at_ms"), run.startedAtMs);

        if (!query.exec())
        {
            return InspectionRunRepositoryUtils::failAndRollback(
                transaction,
                QStringLiteral("Failed to Insert Inspection Run: %1")
                    .arg(SqliteRepositoryUtils::formatSqlError(
                        query.lastError())),
                errorMessage);
        }
        query.finish();
    }

    QSqlQuery itemQuery(database);
    if (!itemQuery.prepare(
            QStringLiteral(
                "INSERT INTO inspection_item ("
                "result_id, run_id, sequence_no, source_file_name, "
                "source_file_path, source_file_size, source_modified_at_ms, "
                "source_file_hash, status, created_at_ms, updated_at_ms"
                ") VALUES ("
                ":result_id, :run_id, :sequence_no, :source_file_name, "
                ":source_file_path, :source_file_size, :source_modified_at_ms, "
                ":source_file_hash, 'PENDING', :created_at_ms, :updated_at_ms"
                ");")))
    {
        return InspectionRunRepositoryUtils::failAndRollback(
            transaction,
            QStringLiteral("Failed to Prepare Inspection Item Insert: %1")
                .arg(SqliteRepositoryUtils::formatSqlError(
                    itemQuery.lastError())),
            errorMessage);
    }

    for (const NewInspectionItem& item : items)
    {
        itemQuery.bindValue(QStringLiteral(":result_id"), item.resultId);
        itemQuery.bindValue(QStringLiteral(":run_id"), run.runId);
        itemQuery.bindValue(QStringLiteral(":sequence_no"), item.sequenceNo);
        itemQuery.bindValue(
            QStringLiteral(":source_file_name"),
            item.sourceFileName);
        itemQuery.bindValue(
            QStringLiteral(":source_file_path"),
            item.sourceFilePath);
        itemQuery.bindValue(
            QStringLiteral(":source_file_size"),
            item.sourceFileSize);
        itemQuery.bindValue(
            QStringLiteral(":source_modified_at_ms"),
            item.sourceModifiedAtMs);
        itemQuery.bindValue(
            QStringLiteral(":source_file_hash"),
            InspectionRunRepositoryUtils::nullableString(
                item.sourceFileHash));
        itemQuery.bindValue(
            QStringLiteral(":created_at_ms"),
            item.createdAtMs);
        itemQuery.bindValue(
            QStringLiteral(":updated_at_ms"),
            item.createdAtMs);

        if (!itemQuery.exec())
        {
            return InspectionRunRepositoryUtils::failAndRollback(
                transaction,
                QStringLiteral(
                    "Failed to Insert Inspection Item at Sequence %1: %2")
                    .arg(item.sequenceNo)
                    .arg(SqliteRepositoryUtils::formatSqlError(
                        itemQuery.lastError())),
                errorMessage);
        }
        itemQuery.finish();
    }

    {
        QSqlQuery query(database);
        query.prepare(
            QStringLiteral(
                "UPDATE inspection_run "
                "SET total_count = :total_count "
                "WHERE run_id = :run_id AND status = 'RUNNING';"));
        query.bindValue(QStringLiteral(":total_count"), items.size());
        query.bindValue(QStringLiteral(":run_id"), run.runId);

        if (!query.exec())
        {
            return InspectionRunRepositoryUtils::failAndRollback(
                transaction,
                QStringLiteral("Failed to Finalize Inspection Run Input Count: %1")
                    .arg(SqliteRepositoryUtils::formatSqlError(
                        query.lastError())),
                errorMessage);
        }

        const qint64 affectedRows = query.numRowsAffected();
        query.finish();
        if (affectedRows != 1)
        {
            return InspectionRunRepositoryUtils::failAndRollback(
                transaction,
                QStringLiteral(
                    "Finalizing Inspection Run Affected %1 Rows.")
                    .arg(affectedRows),
                errorMessage);
        }
    }

    if (!transaction.commit(&transactionError))
    {
        return InspectionRunRepositoryUtils::failAndRollback(
            transaction,
            QStringLiteral("Failed to Commit Inspection Run Creation: %1")
                .arg(transactionError),
            errorMessage);
    }

    return true;
}

std::optional<RecoveryReport> InspectionRunRepository::recoverInterruptedRuns(
    QSqlDatabase& database,
    qint64 recoveredAtMs,
    QString* errorMessage)
{
    if (errorMessage != nullptr)
    {
        errorMessage->clear();
    }

    if (!SqliteRepositoryUtils::validateOpenSqliteDatabase(
            database,
            errorMessage)
        || recoveredAtMs <= 0)
    {
        if (recoveredAtMs <= 0)
        {
            InspectionRunRepositoryUtils::setErrorMessage(
                errorMessage,
                QStringLiteral("Recovery Timestamp Must be Positive."));
        }
        return std::nullopt;
    }

    SqliteImmediateTransaction transaction(database);
    QString transactionError;
    if (!transaction.begin(&transactionError))
    {
        InspectionRunRepositoryUtils::setErrorMessage(
            errorMessage,
            transactionError);
        return std::nullopt;
    }

    RecoveryReport report;

    {
        QSqlQuery query(database);
        query.prepare(
            QStringLiteral(
                "UPDATE inspection_item "
                "SET status = 'PENDING', "
                "processing_started_at_ms = NULL, "
                "updated_at_ms = :recovered_at_ms "
                "WHERE status = 'PROCESSING' "
                "AND run_id IN ("
                "SELECT run_id FROM inspection_run WHERE status = 'RUNNING'"
                ");"));
        query.bindValue(QStringLiteral(":recovered_at_ms"), recoveredAtMs);

        if (!query.exec())
        {
            InspectionRunRepositoryUtils::failAndRollback(
                transaction,
                QStringLiteral("Failed to Reset Processing Items: %1")
                    .arg(SqliteRepositoryUtils::formatSqlError(
                        query.lastError())),
                errorMessage);
            return std::nullopt;
        }

        report.resetItemCount = query.numRowsAffected();
        query.finish();
    }

    {
        QSqlQuery query(database);
        query.prepare(
            QStringLiteral(
                "UPDATE inspection_run "
                "SET status = 'INTERRUPTED', "
                "last_interrupted_at_ms = :recovered_at_ms, "
                "updated_at_ms = :recovered_at_ms, "
                "revision = revision + 1 "
                "WHERE status = 'RUNNING';"));
        query.bindValue(QStringLiteral(":recovered_at_ms"), recoveredAtMs);

        if (!query.exec())
        {
            InspectionRunRepositoryUtils::failAndRollback(
                transaction,
                QStringLiteral("Failed to Interrupt Running Batches: %1")
                    .arg(SqliteRepositoryUtils::formatSqlError(
                        query.lastError())),
                errorMessage);
            return std::nullopt;
        }

        report.interruptedRunCount = query.numRowsAffected();
        query.finish();
    }

    if (!transaction.commit(&transactionError))
    {
        InspectionRunRepositoryUtils::failAndRollback(
            transaction,
            QStringLiteral("Failed to Commit Startup Recovery: %1")
                .arg(transactionError),
            errorMessage);
        return std::nullopt;
    }

    return report;
}

bool InspectionRunRepository::resumeInterruptedRun(
    QSqlDatabase& database,
    const QString& runId,
    qint64 resumedAtMs,
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
        || resumedAtMs <= 0)
    {
        if (errorMessage != nullptr && errorMessage->isEmpty())
        {
            *errorMessage = QStringLiteral("Run ID or Resume Timestamp is Invalid.");
        }
        return false;
    }

    SqliteImmediateTransaction transaction(database);
    QString transactionError;
    if (!transaction.begin(&transactionError))
    {
        InspectionRunRepositoryUtils::setErrorMessage(
            errorMessage,
            transactionError);
        return false;
    }

    QSqlQuery query(database);
    query.prepare(
        QStringLiteral(
            "UPDATE inspection_run "
            "SET status = 'RUNNING', "
            "updated_at_ms = :resumed_at_ms, "
            "revision = revision + 1 "
            "WHERE run_id = :run_id AND status = 'INTERRUPTED';"));
    query.bindValue(QStringLiteral(":resumed_at_ms"), resumedAtMs);
    query.bindValue(QStringLiteral(":run_id"), runId);

    if (!query.exec())
    {
        return InspectionRunRepositoryUtils::failAndRollback(
            transaction,
            QStringLiteral("Failed to Resume Inspection Run: %1")
                .arg(SqliteRepositoryUtils::formatSqlError(
                    query.lastError())),
            errorMessage);
    }

    const qint64 affectedRows = query.numRowsAffected();
    query.finish();
    if (affectedRows != 1)
    {
        return InspectionRunRepositoryUtils::failAndRollback(
            transaction,
            QStringLiteral(
                "Resume Requires Exactly One INTERRUPTED Run; Affected=%1")
                .arg(affectedRows),
            errorMessage);
    }

    if (!transaction.commit(&transactionError))
    {
        return InspectionRunRepositoryUtils::failAndRollback(
            transaction,
            QStringLiteral("Failed to Commit Run Resume: %1")
                .arg(transactionError),
            errorMessage);
    }

    return true;
}

bool InspectionRunRepository::finishAsDraft(
    QSqlDatabase& database,
    const QString& runId,
    qint64 completedAtMs,
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
        || completedAtMs <= 0)
    {
        if (errorMessage != nullptr && errorMessage->isEmpty())
        {
            *errorMessage = QStringLiteral(
                "Run ID or Completion Timestamp is Invalid.");
        }
        return false;
    }

    SqliteImmediateTransaction transaction(database);
    QString transactionError;
    if (!transaction.begin(&transactionError))
    {
        InspectionRunRepositoryUtils::setErrorMessage(
            errorMessage,
            transactionError);
        return false;
    }

    QSqlQuery query(database);
    query.prepare(
        QStringLiteral(
            "UPDATE inspection_run "
            "SET "
            "completed_count = ("
            "  SELECT COUNT(*) FROM inspection_item "
            "  WHERE run_id = :run_id "
            "    AND status IN ('SUCCESS', 'FAILED')"
            "), "
            "failed_count = ("
            "  SELECT COUNT(*) FROM inspection_item "
            "  WHERE run_id = :run_id AND status = 'FAILED'"
            "), "
            "ok_count = ("
            "  SELECT COUNT(*) FROM inspection_item "
            "  WHERE run_id = :run_id AND detection_level = 'OK'"
            "), "
            "review_count = ("
            "  SELECT COUNT(*) FROM inspection_item "
            "  WHERE run_id = :run_id AND detection_level = 'REVIEW'"
            "), "
            "ng_count = ("
            "  SELECT COUNT(*) FROM inspection_item "
            "  WHERE run_id = :run_id AND detection_level = 'NG'"
            "), "
            "score_sum = COALESCE(("
            "  SELECT SUM(score) FROM inspection_item "
            "  WHERE run_id = :run_id AND status = 'SUCCESS'"
            "), 0), "
            "status = 'DRAFT', "
            "completed_at_ms = :completed_at_ms, "
            "updated_at_ms = :completed_at_ms, "
            "revision = revision + 1 "
            "WHERE run_id = :run_id AND status = 'RUNNING';"));
    query.bindValue(QStringLiteral(":run_id"), runId);
    query.bindValue(QStringLiteral(":completed_at_ms"), completedAtMs);

    if (!query.exec())
    {
        return InspectionRunRepositoryUtils::failAndRollback(
            transaction,
            QStringLiteral("Failed to Finish Inspection Run as Draft: %1")
                .arg(SqliteRepositoryUtils::formatSqlError(
                    query.lastError())),
            errorMessage);
    }

    const qint64 affectedRows = query.numRowsAffected();
    query.finish();
    if (affectedRows != 1)
    {
        return InspectionRunRepositoryUtils::failAndRollback(
            transaction,
            QStringLiteral(
                "Finishing Run Requires Exactly One RUNNING Row; Affected=%1")
                .arg(affectedRows),
            errorMessage);
    }

    if (!transaction.commit(&transactionError))
    {
        return InspectionRunRepositoryUtils::failAndRollback(
            transaction,
            QStringLiteral("Failed to Commit Draft Completion: %1")
                .arg(transactionError),
            errorMessage);
    }

    return true;
}

bool InspectionRunRepository::saveDraft(
    QSqlDatabase& database,
    const SaveInspectionRunRequest& request,
    QString* errorMessage)
{
    if (errorMessage != nullptr)
    {
        errorMessage->clear();
    }

    const bool validHashes =
        MigrationCheckSum::isValidSha256Hex(request.manifestSha256)
        && MigrationCheckSum::isValidSha256Hex(request.itemsSha256);

    if (!SqliteRepositoryUtils::validateOpenSqliteDatabase(
            database,
            errorMessage)
        || request.runId.size() != 36
        || request.name.trimmed().isEmpty()
        || request.name.trimmed().size() > 120
        || (request.note.has_value() && request.note->size() > 4000)
        || request.savedAtMs <= 0
        || request.manifestVersion <= 0
        || request.manifestRelativePath.trimmed().isEmpty()
        || request.itemsRelativePath.trimmed().isEmpty()
        || !validHashes)
    {
        if (errorMessage != nullptr && errorMessage->isEmpty())
        {
            *errorMessage = QStringLiteral("Save Run Request is Invalid.");
        }
        return false;
    }

    SqliteImmediateTransaction transaction(database);
    QString transactionError;
    if (!transaction.begin(&transactionError))
    {
        InspectionRunRepositoryUtils::setErrorMessage(
            errorMessage,
            transactionError);
        return false;
    }

    QString currentStatus;
    QString currentManifestHash;
    QString currentItemsHash;

    {
        QSqlQuery query(database);
        query.prepare(
            QStringLiteral(
                "SELECT status, manifest_sha256, items_sha256 "
                "FROM inspection_run WHERE run_id = :run_id;"));
        query.bindValue(QStringLiteral(":run_id"), request.runId);

        if (!query.exec())
        {
            return InspectionRunRepositoryUtils::failAndRollback(
                transaction,
                QStringLiteral("Failed to Read Run Before Saving: %1")
                    .arg(SqliteRepositoryUtils::formatSqlError(
                        query.lastError())),
                errorMessage);
        }

        if (!query.next())
        {
            query.finish();
            return InspectionRunRepositoryUtils::failAndRollback(
                transaction,
                QStringLiteral("Inspection Run Does not Exist: %1")
                    .arg(request.runId),
                errorMessage);
        }

        currentStatus = query.value(0).toString();
        currentManifestHash = query.value(1).toString();
        currentItemsHash = query.value(2).toString();
        query.finish();
    }

    if (currentStatus == QStringLiteral("SAVED"))
    {
        if (currentManifestHash == request.manifestSha256
            && currentItemsHash == request.itemsSha256)
        {
            if (!transaction.commit(&transactionError))
            {
                return InspectionRunRepositoryUtils::failAndRollback(
                    transaction,
                    QStringLiteral("Failed to Finish Idempotent Save: %1")
                        .arg(transactionError),
                    errorMessage);
            }
            return true;
        }

        return InspectionRunRepositoryUtils::failAndRollback(
            transaction,
            QStringLiteral(
                "Saved Run Manifest Conflict for Run %1.")
                .arg(request.runId),
            errorMessage);
    }

    if (currentStatus != QStringLiteral("DRAFT"))
    {
        return InspectionRunRepositoryUtils::failAndRollback(
            transaction,
            QStringLiteral(
                "Only a DRAFT Run Can be Saved; Run=%1, Status=%2")
                .arg(request.runId)
                .arg(currentStatus),
            errorMessage);
    }

    QSqlQuery query(database);
    query.prepare(
        QStringLiteral(
            "UPDATE inspection_run "
            "SET status = 'SAVED', "
            "name = :name, note = :note, saved_at_ms = :saved_at_ms, "
            "manifest_version = :manifest_version, "
            "manifest_rel_path = :manifest_rel_path, "
            "manifest_sha256 = :manifest_sha256, "
            "items_rel_path = :items_rel_path, "
            "items_sha256 = :items_sha256, "
            "updated_at_ms = :saved_at_ms, "
            "revision = revision + 1 "
            "WHERE run_id = :run_id AND status = 'DRAFT';"));
    query.bindValue(QStringLiteral(":name"), request.name.trimmed());
    query.bindValue(
        QStringLiteral(":note"),
        InspectionRunRepositoryUtils::nullableString(request.note));
    query.bindValue(QStringLiteral(":saved_at_ms"), request.savedAtMs);
    query.bindValue(
        QStringLiteral(":manifest_version"),
        request.manifestVersion);
    query.bindValue(
        QStringLiteral(":manifest_rel_path"),
        request.manifestRelativePath);
    query.bindValue(
        QStringLiteral(":manifest_sha256"),
        request.manifestSha256);
    query.bindValue(
        QStringLiteral(":items_rel_path"),
        request.itemsRelativePath);
    query.bindValue(
        QStringLiteral(":items_sha256"),
        request.itemsSha256);
    query.bindValue(QStringLiteral(":run_id"), request.runId);

    if (!query.exec())
    {
        return InspectionRunRepositoryUtils::failAndRollback(
            transaction,
            QStringLiteral("Failed to Save Inspection Run: %1")
                .arg(SqliteRepositoryUtils::formatSqlError(
                    query.lastError())),
            errorMessage);
    }

    const qint64 affectedRows = query.numRowsAffected();
    query.finish();
    if (affectedRows != 1)
    {
        return InspectionRunRepositoryUtils::failAndRollback(
            transaction,
            QStringLiteral("Saving Draft Affected %1 Rows.").arg(affectedRows),
            errorMessage);
    }

    if (!transaction.commit(&transactionError))
    {
        return InspectionRunRepositoryUtils::failAndRollback(
            transaction,
            QStringLiteral("Failed to Commit Run Save: %1")
                .arg(transactionError),
            errorMessage);
    }

    return true;
}

bool InspectionRunRepository::discardDraft(
    QSqlDatabase& database,
    const QString& runId,
    qint64 createdAtMs,
    QString* errorMessage)
{
    return InspectionRunRepositoryUtils::deleteRun(
        database,
        runId,
        QStringLiteral("DRAFT"),
        QStringLiteral("DISCARD_DRAFT"),
        createdAtMs,
        errorMessage);
}

bool InspectionRunRepository::deleteSaved(
    QSqlDatabase& database,
    const QString& runId,
    qint64 createdAtMs,
    QString* errorMessage)
{
    return InspectionRunRepositoryUtils::deleteRun(
        database,
        runId,
        QStringLiteral("SAVED"),
        QStringLiteral("DELETE_SAVED"),
        createdAtMs,
        errorMessage);
}

std::optional<QVector<InspectionRunHistoryRow>>
InspectionRunRepository::listSavedHistory(
    const QSqlDatabase& database,
    int pageSize,
    std::optional<qint64> beforeSavedAtMs,
    const QString& beforeRunId,
    QString* errorMessage)
{
    if (errorMessage != nullptr)
    {
        errorMessage->clear();
    }

    if (!SqliteRepositoryUtils::validateOpenSqliteDatabase(
            database,
            errorMessage))
    {
        return std::nullopt;
    }

    if (pageSize < 1 || pageSize > 500)
    {
        InspectionRunRepositoryUtils::setErrorMessage(
            errorMessage,
            QStringLiteral("History Page Size Must be Between 1 and 500."));
        return std::nullopt;
    }

    const bool hasCursor = beforeSavedAtMs.has_value();
    if (hasCursor
        && (beforeSavedAtMs.value() <= 0 || beforeRunId.size() != 36))
    {
        InspectionRunRepositoryUtils::setErrorMessage(
            errorMessage,
            QStringLiteral("History Cursor is Invalid."));
        return std::nullopt;
    }

    QSqlQuery query(database);
    const QString baseSelect =
        QStringLiteral(
            "SELECT "
            "run_id, name, note, source_type, source_path, "
            "model_format, model_path, started_at_ms, completed_at_ms, "
            "saved_at_ms, total_count, completed_count, failed_count, "
            "ok_count, review_count, ng_count, average_score "
            "FROM v_inspection_run_history ");

    const QString statement =
        hasCursor
        ? baseSelect
              + QStringLiteral(
                  "WHERE saved_at_ms < :saved_at_ms "
                  "OR (saved_at_ms = :saved_at_ms AND run_id < :run_id) "
                  "ORDER BY saved_at_ms DESC, run_id DESC "
                  "LIMIT :page_size;")
        : baseSelect
              + QStringLiteral(
                  "ORDER BY saved_at_ms DESC, run_id DESC "
                  "LIMIT :page_size;");

    if (!query.prepare(statement))
    {
        InspectionRunRepositoryUtils::setErrorMessage(
            errorMessage,
            QStringLiteral("Failed to Prepare History Query: %1")
                .arg(SqliteRepositoryUtils::formatSqlError(
                    query.lastError())));
        return std::nullopt;
    }

    if (hasCursor)
    {
        query.bindValue(
            QStringLiteral(":saved_at_ms"),
            beforeSavedAtMs.value());
        query.bindValue(QStringLiteral(":run_id"), beforeRunId);
    }
    query.bindValue(QStringLiteral(":page_size"), pageSize);

    if (!query.exec())
    {
        InspectionRunRepositoryUtils::setErrorMessage(
            errorMessage,
            QStringLiteral("Failed to Query Saved Run History: %1")
                .arg(SqliteRepositoryUtils::formatSqlError(
                    query.lastError())));
        return std::nullopt;
    }

    QVector<InspectionRunHistoryRow> rows;
    while (query.next())
    {
        InspectionRunHistoryRow row;
        row.runId = query.value(0).toString();
        row.name = query.value(1).toString();
        if (!query.value(2).isNull())
        {
            row.note = query.value(2).toString();
        }
        row.sourceType = query.value(3).toString();
        row.sourcePath = query.value(4).toString();
        row.modelFormat = query.value(5).toString();
        row.modelPath = query.value(6).toString();
        row.startedAtMs = query.value(7).toLongLong();
        row.completedAtMs = query.value(8).toLongLong();
        row.savedAtMs = query.value(9).toLongLong();
        row.totalCount = query.value(10).toLongLong();
        row.completedCount = query.value(11).toLongLong();
        row.failedCount = query.value(12).toLongLong();
        row.okCount = query.value(13).toLongLong();
        row.reviewCount = query.value(14).toLongLong();
        row.ngCount = query.value(15).toLongLong();
        if (!query.value(16).isNull())
        {
            row.averageScore = query.value(16).toDouble();
        }
        rows.push_back(std::move(row));
    }

    if (query.lastError().isValid())
    {
        InspectionRunRepositoryUtils::setErrorMessage(
            errorMessage,
            QStringLiteral("Failed While Reading Saved Run History: %1")
                .arg(SqliteRepositoryUtils::formatSqlError(
                    query.lastError())));
        return std::nullopt;
    }

    query.finish();
    return rows;
}
