#include "PersistenceWorker.h"

#include "InspectionItemRepository.h"
#include "InspectionRunRepository.h"
#include "MigrationCheckSum.h"
#include "SqliteTransaction.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>
#include <QVariant>

namespace PersistenceWorkerUtils
{

qint64 currentTimeMs()
{
    return QDateTime::currentMSecsSinceEpoch();
}

QString sha256File(const QString& path, QString* errorMessage)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        if (errorMessage != nullptr)
        {
            *errorMessage = QStringLiteral("Failed to Open Model for Hashing: %1")
                                .arg(file.errorString());
        }
        return {};
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);
    while (!file.atEnd())
    {
        const QByteArray chunk = file.read(1024 * 1024);
        if (chunk.isEmpty() && file.error() != QFileDevice::NoError)
        {
            if (errorMessage != nullptr)
            {
                *errorMessage =
                    QStringLiteral("Failed While Hashing Model: %1")
                        .arg(file.errorString());
            }
            return {};
        }
        hash.addData(chunk);
    }

    return QString::fromLatin1(hash.result().toHex());
}

QString detectionLevelText(DetectionLevel level)
{
    switch (level)
    {
    case DetectionLevel::OK:
        return QStringLiteral("OK");
    case DetectionLevel::REVIEW:
        return QStringLiteral("REVIEW");
    case DetectionLevel::NG:
        return QStringLiteral("NG");
    }

    return {};
}

bool writeAndHash(
    QSaveFile& file,
    QCryptographicHash& hash,
    const QByteArray& bytes,
    QString* errorMessage)
{
    if (file.write(bytes) != bytes.size())
    {
        if (errorMessage != nullptr)
        {
            *errorMessage = QStringLiteral("Failed to Write JSON File: %1")
                                .arg(file.errorString());
        }
        return false;
    }

    hash.addData(bytes);
    return true;
}

QJsonObject itemJsonObject(const QSqlQuery& query)
{
    QJsonObject object;
    object.insert(QStringLiteral("result_id"), query.value(0).toString());
    object.insert(QStringLiteral("sequence_no"), query.value(1).toInt());
    object.insert(
        QStringLiteral("source_file_name"),
        query.value(2).toString());
    object.insert(
        QStringLiteral("source_file_path"),
        query.value(3).toString());
    object.insert(QStringLiteral("status"), query.value(4).toString());

    if (!query.value(5).isNull())
    {
        object.insert(
            QStringLiteral("detection_level"),
            query.value(5).toString());
    }
    if (!query.value(6).isNull())
    {
        object.insert(QStringLiteral("score"), query.value(6).toDouble());
    }
    if (!query.value(7).isNull())
    {
        object.insert(
            QStringLiteral("inference_ms"),
            query.value(7).toLongLong());
    }
    if (!query.value(8).isNull())
    {
        object.insert(
            QStringLiteral("defect_area"),
            query.value(8).toLongLong());
    }
    if (!query.value(9).isNull())
    {
        object.insert(
            QStringLiteral("heatmap_relative_path"),
            query.value(9).toString());
    }
    if (!query.value(10).isNull())
    {
        object.insert(
            QStringLiteral("error_message"),
            query.value(10).toString());
    }

    return object;
}

}

PersistenceWorker::PersistenceWorker(
    QString dataBaseFilePath,
    QString managedRunsRoot,
    QString connectionName,
    QObject* parent)
    : QObject(parent)
    , dataBaseFilePath_(QDir::cleanPath(std::move(dataBaseFilePath)))
    , managedRunsRoot_(QDir::cleanPath(std::move(managedRunsRoot)))
    , connectionName_(std::move(connectionName))
{
}

void PersistenceWorker::initialize()
{
    if (ready_)
    {
        emit initialized(true, {});
        return;
    }

    QString errorMessage;
    auto connection = DataBaseConnection::openSqlite(
        dataBaseFilePath_,
        connectionName_,
        &errorMessage);
    if (!connection.has_value())
    {
        emit initialized(false, errorMessage);
        return;
    }

    connection_.emplace(std::move(connection.value()));
    ready_ = true;

    {
        QSqlDatabase database = dataBase();
        processCleanupJobs(database);
        discardStartupDrafts(database);
        processCleanupJobs(database);
    }

    emit initialized(true, {});

    {
        QSqlDatabase database = dataBase();
        const auto catalog = readBatchCatalog(database, &errorMessage);
        if (!catalog.has_value())
        {
            emit operationFailed(
                QStringLiteral("list_interrupted_runs"),
                {},
                errorMessage);
        }
        else
        {
            InspectionRunRecords interrupted;
            for (const InspectionRunRecord& run : catalog.value())
            {
                if (run.status == QStringLiteral("INTERRUPTED"))
                {
                    interrupted.push_back(run);
                }
            }
            emit interruptedRunsReady(interrupted);
        }
    }
}

void PersistenceWorker::shutdown()
{
    if (connection_.has_value())
    {
        {
            QSqlDatabase database = dataBase();
            processCleanupJobs(database);
        }
        ready_ = false;
        connection_.reset();
    }
}

bool PersistenceWorker::ensureReady(
    const QString& operation,
    const QString& contextId)
{
    if (ready_ && connection_.has_value() && connection_->isOpen())
    {
        return true;
    }

    emit operationFailed(
        operation,
        contextId,
        QStringLiteral("Persistence Worker is not Initialized."));
    return false;
}

QSqlDatabase PersistenceWorker::dataBase() const
{
    return QSqlDatabase::database(connectionName_, false);
}

void PersistenceWorker::createRun(NewInspectionRun run)
{
    if (!ensureReady(QStringLiteral("create_run"), run.runId))
    {
        return;
    }

    QString errorMessage;
    qint64 totalCount = 0;
    QSqlDatabase database = dataBase();

    if (!createRunFromSource(
            database,
            &run,
            &totalCount,
            &errorMessage))
    {
        if (isManagedRunPath(run.storagePath))
        {
            QDir(run.storagePath).removeRecursively();
        }
        emit operationFailed(
            QStringLiteral("create_run"),
            run.runId,
            errorMessage);
        return;
    }

    const auto storedRun = readRun(database, run.runId, &errorMessage);
    if (!storedRun.has_value())
    {
        emit operationFailed(
            QStringLiteral("read_created_run"),
            run.runId,
            errorMessage);
        return;
    }

    Q_UNUSED(totalCount);
    emit runCreated(storedRun.value());
}

bool PersistenceWorker::createRunFromSource(
    QSqlDatabase& database,
    NewInspectionRun* run,
    qint64* totalCount,
    QString* errorMessage)
{
    if (run == nullptr || totalCount == nullptr)
    {
        if (errorMessage != nullptr)
        {
            *errorMessage = QStringLiteral("Run Creation Arguments are Invalid.");
        }
        return false;
    }

    if (run->runId.size() != 36
        || (run->sourceType != QStringLiteral("SINGLE_IMAGE")
            && run->sourceType != QStringLiteral("FOLDER"))
        || run->sourcePath.trimmed().isEmpty()
        || run->modelPath.trimmed().isEmpty()
        || run->reviewThreshold < 0.0
        || run->ngThreshold > 1.0
        || run->reviewThreshold >= run->ngThreshold
        || run->heatmapAlpha < 0.0
        || run->heatmapAlpha > 1.0)
    {
        if (errorMessage != nullptr)
        {
            *errorMessage = QStringLiteral("Run Metadata is Invalid.");
        }
        return false;
    }

    const QFileInfo modelInfo(run->modelPath);
    if (!modelInfo.exists() || !modelInfo.isFile())
    {
        if (errorMessage != nullptr)
        {
            *errorMessage = QStringLiteral("Model File Does not Exist: %1")
                                .arg(run->modelPath);
        }
        return false;
    }

    const QString modelSuffix = modelInfo.suffix().toLower();
    if (modelSuffix != QStringLiteral("onnx")
        && modelSuffix != QStringLiteral("xml"))
    {
        if (errorMessage != nullptr)
        {
            *errorMessage = QStringLiteral("Unsupported Model Format: %1")
                                .arg(modelSuffix);
        }
        return false;
    }

    run->modelPath = QDir::cleanPath(modelInfo.absoluteFilePath());
    run->modelFormat =
        modelSuffix == QStringLiteral("onnx")
            ? QStringLiteral("ONNX")
            : QStringLiteral("OPENVINO");
    run->modelHash =
        PersistenceWorkerUtils::sha256File(run->modelPath, errorMessage);
    if (!MigrationCheckSum::isValidSha256Hex(run->modelHash))
    {
        return false;
    }

    run->sourcePath =
        QDir::cleanPath(QFileInfo(run->sourcePath).absoluteFilePath());
    run->storageOrigin = QStringLiteral("MANAGED");
    run->storagePath = QDir::cleanPath(run->storagePath);
    run->startedAtMs =
        run->startedAtMs > 0
            ? run->startedAtMs
            : PersistenceWorkerUtils::currentTimeMs();
    if (run->appVersion.trimmed().isEmpty())
    {
        run->appVersion = QStringLiteral("development");
    }

    if (!isManagedRunPath(run->storagePath))
    {
        if (errorMessage != nullptr)
        {
            *errorMessage =
                QStringLiteral("Run Storage Path is Outside the Managed Root.");
        }
        return false;
    }

    const QString heatmapDirectory =
        QDir(run->storagePath).filePath(QStringLiteral("heatmaps"));
    if (!QDir().mkpath(heatmapDirectory))
    {
        if (errorMessage != nullptr)
        {
            *errorMessage = QStringLiteral("Failed to Create Run Directory: %1")
                                .arg(run->storagePath);
        }
        return false;
    }

    SqliteImmediateTransaction transaction(database);
    QString transactionError;
    if (!transaction.begin(&transactionError))
    {
        if (errorMessage != nullptr)
        {
            *errorMessage = transactionError;
        }
        return false;
    }

    auto rollbackFailure =
        [&transaction, errorMessage](const QString& message)
    {
        QString rollbackError;
        (void)transaction.rollback(&rollbackError);
        if (errorMessage != nullptr)
        {
            *errorMessage =
                rollbackError.isEmpty()
                    ? message
                    : QStringLiteral("%1 Rollback also Failed: %2")
                          .arg(message, rollbackError);
        }
        return false;
    };

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
                ":storage_path, 'MANAGED', :model_path, :model_format, "
                ":model_hash, :app_version, :review_threshold, :ng_threshold, "
                ":heatmap_alpha, :started_at_ms, :started_at_ms, 0"
                ");"));
        query.bindValue(QStringLiteral(":run_id"), run->runId);
        query.bindValue(QStringLiteral(":source_type"), run->sourceType);
        query.bindValue(QStringLiteral(":source_path"), run->sourcePath);
        query.bindValue(QStringLiteral(":storage_path"), run->storagePath);
        query.bindValue(QStringLiteral(":model_path"), run->modelPath);
        query.bindValue(QStringLiteral(":model_format"), run->modelFormat);
        query.bindValue(QStringLiteral(":model_hash"), run->modelHash);
        query.bindValue(QStringLiteral(":app_version"), run->appVersion);
        query.bindValue(
            QStringLiteral(":review_threshold"),
            run->reviewThreshold);
        query.bindValue(QStringLiteral(":ng_threshold"), run->ngThreshold);
        query.bindValue(QStringLiteral(":heatmap_alpha"), run->heatmapAlpha);
        query.bindValue(QStringLiteral(":started_at_ms"), run->startedAtMs);

        if (!query.exec())
        {
            return rollbackFailure(
                QStringLiteral("Failed to Insert Inspection Run: %1")
                    .arg(SqliteRepositoryUtils::formatSqlError(
                        query.lastError())));
        }
    }

    QSqlQuery itemQuery(database);
    if (!itemQuery.prepare(
            QStringLiteral(
                "INSERT INTO inspection_item ("
                "result_id, run_id, sequence_no, source_file_name, "
                "source_file_path, source_file_size, source_modified_at_ms, "
                "status, created_at_ms, updated_at_ms"
                ") VALUES ("
                ":result_id, :run_id, :sequence_no, :source_file_name, "
                ":source_file_path, :source_file_size, "
                ":source_modified_at_ms, 'PENDING', "
                ":created_at_ms, :created_at_ms"
                ");")))
    {
        return rollbackFailure(
            QStringLiteral("Failed to Prepare Inspection Item Insert: %1")
                .arg(SqliteRepositoryUtils::formatSqlError(
                    itemQuery.lastError())));
    }

    qint64 sequenceNo = 0;
    auto insertFile =
        [&](const QString& path)
    {
        const QFileInfo fileInfo(path);
        if (!fileInfo.exists() || !fileInfo.isFile())
        {
            return true;
        }

        ++sequenceNo;
        const qint64 createdAtMs = PersistenceWorkerUtils::currentTimeMs();
        itemQuery.bindValue(
            QStringLiteral(":result_id"),
            QUuid::createUuid().toString(QUuid::WithoutBraces));
        itemQuery.bindValue(QStringLiteral(":run_id"), run->runId);
        itemQuery.bindValue(QStringLiteral(":sequence_no"), sequenceNo);
        itemQuery.bindValue(
            QStringLiteral(":source_file_name"),
            fileInfo.fileName());
        itemQuery.bindValue(
            QStringLiteral(":source_file_path"),
            QDir::cleanPath(fileInfo.absoluteFilePath()));
        itemQuery.bindValue(
            QStringLiteral(":source_file_size"),
            fileInfo.size());
        itemQuery.bindValue(
            QStringLiteral(":source_modified_at_ms"),
            qMax<qint64>(0, fileInfo.lastModified().toMSecsSinceEpoch()));
        itemQuery.bindValue(
            QStringLiteral(":created_at_ms"),
            createdAtMs);

        if (!itemQuery.exec())
        {
            if (errorMessage != nullptr)
            {
                *errorMessage =
                    QStringLiteral(
                        "Failed to Insert Inspection Item %1: %2")
                        .arg(sequenceNo)
                        .arg(SqliteRepositoryUtils::formatSqlError(
                            itemQuery.lastError()));
            }
            return false;
        }
        itemQuery.finish();
        return true;
    };

    if (run->sourceType == QStringLiteral("SINGLE_IMAGE"))
    {
        if (!insertFile(run->sourcePath))
        {
            return rollbackFailure(
                errorMessage != nullptr
                    ? *errorMessage
                    : QStringLiteral("Failed to Insert Input Image."));
        }
    }
    else
    {
        const QFileInfo folderInfo(run->sourcePath);
        if (!folderInfo.exists() || !folderInfo.isDir())
        {
            return rollbackFailure(
                QStringLiteral("Input Folder Does not Exist: %1")
                    .arg(run->sourcePath));
        }

        QDirIterator iterator(
            run->sourcePath,
            {
                QStringLiteral("*.png"),
                QStringLiteral("*.jpg"),
                QStringLiteral("*.jpeg"),
                QStringLiteral("*.bmp")
            },
            QDir::Files,
            QDirIterator::Subdirectories);
        while (iterator.hasNext())
        {
            if (!insertFile(iterator.next()))
            {
                return rollbackFailure(
                    errorMessage != nullptr
                        ? *errorMessage
                        : QStringLiteral("Failed to Insert Folder Image."));
            }
        }
    }

    if (sequenceNo == 0)
    {
        return rollbackFailure(
            QStringLiteral("The Selected Input Contains no Supported Images."));
    }

    {
        QSqlQuery query(database);
        query.prepare(
            QStringLiteral(
                "UPDATE inspection_run "
                "SET total_count = :total_count "
                "WHERE run_id = :run_id AND status = 'RUNNING';"));
        query.bindValue(QStringLiteral(":total_count"), sequenceNo);
        query.bindValue(QStringLiteral(":run_id"), run->runId);
        if (!query.exec() || query.numRowsAffected() != 1)
        {
            return rollbackFailure(
                QStringLiteral("Failed to Finalize Run Input Count: %1")
                    .arg(SqliteRepositoryUtils::formatSqlError(
                        query.lastError())));
        }
    }

    if (!transaction.commit(&transactionError))
    {
        return rollbackFailure(
            QStringLiteral("Failed to Commit Run Creation: %1")
                .arg(transactionError));
    }

    *totalCount = sequenceNo;
    return true;
}

std::optional<InspectionRunRecord> PersistenceWorker::readRun(
    const QSqlDatabase& database,
    const QString& runId,
    QString* errorMessage) const
{
    QSqlQuery query(database);
    query.prepare(
        QStringLiteral(
            "SELECT "
            "run_id, name, note, status, source_type, source_path, "
            "storage_path, storage_origin, model_path, model_format, "
            "model_hash, app_version, review_threshold, ng_threshold, "
            "heatmap_alpha, started_at_ms, completed_at_ms, saved_at_ms, "
            "total_count, completed_count, failed_count, ok_count, "
            "review_count, ng_count, score_sum "
            "FROM inspection_run WHERE run_id = :run_id;"));
    query.bindValue(QStringLiteral(":run_id"), runId);
    if (!query.exec())
    {
        if (errorMessage != nullptr)
        {
            *errorMessage = QStringLiteral("Failed to Read Inspection Run: %1")
                                .arg(SqliteRepositoryUtils::formatSqlError(
                                    query.lastError()));
        }
        return std::nullopt;
    }
    if (!query.next())
    {
        if (errorMessage != nullptr)
        {
            *errorMessage = QStringLiteral("Inspection Run Does not Exist: %1")
                                .arg(runId);
        }
        return std::nullopt;
    }

    InspectionRunRecord run;
    run.runId = query.value(0).toString();
    if (!query.value(1).isNull())
    {
        run.name = query.value(1).toString();
    }
    if (!query.value(2).isNull())
    {
        run.note = query.value(2).toString();
    }
    run.status = query.value(3).toString();
    run.sourceType = query.value(4).toString();
    run.sourcePath = query.value(5).toString();
    run.storagePath = query.value(6).toString();
    run.storageOrigin = query.value(7).toString();
    run.modelPath = query.value(8).toString();
    run.modelFormat = query.value(9).toString();
    run.modelHash = query.value(10).toString();
    run.appVersion = query.value(11).toString();
    run.reviewThreshold = query.value(12).toDouble();
    run.ngThreshold = query.value(13).toDouble();
    run.heatmapAlpha = query.value(14).toDouble();
    run.startedAtMs = query.value(15).toLongLong();
    if (!query.value(16).isNull())
    {
        run.completedAtMs = query.value(16).toLongLong();
    }
    if (!query.value(17).isNull())
    {
        run.savedAtMs = query.value(17).toLongLong();
    }
    run.totalCount = query.value(18).toLongLong();
    run.completedCount = query.value(19).toLongLong();
    run.failedCount = query.value(20).toLongLong();
    run.okCount = query.value(21).toLongLong();
    run.reviewCount = query.value(22).toLongLong();
    run.ngCount = query.value(23).toLongLong();
    run.scoreSum = query.value(24).toDouble();
    return run;
}

std::optional<InspectionItemRows> PersistenceWorker::readAllItems(
    const QSqlDatabase& database,
    const QString& runId,
    QString* errorMessage) const
{
    InspectionItemRows allItems;
    int lastSequenceNo = 0;

    while (true)
    {
        const auto page = InspectionItemRepository::listPage(
            database,
            runId,
            lastSequenceNo,
            1000,
            errorMessage);
        if (!page.has_value())
        {
            return std::nullopt;
        }
        if (page->isEmpty())
        {
            break;
        }

        lastSequenceNo = page->last().sequenceNo;
        allItems += page.value();
        if (page->size() < 1000)
        {
            break;
        }
    }

    return allItems;
}

std::optional<InspectionRunRecords> PersistenceWorker::readBatchCatalog(
    const QSqlDatabase& database,
    QString* errorMessage) const
{
    QSqlQuery query(database);
    if (!query.exec(
            QStringLiteral(
                "SELECT run_id FROM inspection_run "
                "WHERE status IN ('SAVED', 'INTERRUPTED') "
                "ORDER BY COALESCE(saved_at_ms, last_interrupted_at_ms, "
                "started_at_ms) DESC, run_id ASC;")))
    {
        if (errorMessage != nullptr)
        {
            *errorMessage = QStringLiteral("Failed to Read Batch Catalog: %1")
                                .arg(SqliteRepositoryUtils::formatSqlError(
                                    query.lastError()));
        }
        return std::nullopt;
    }

    QVector<QString> runIds;
    while (query.next())
    {
        runIds.push_back(query.value(0).toString());
    }
    query.finish();

    InspectionRunRecords runs;
    for (const QString& runId : runIds)
    {
        const auto run = readRun(database, runId, errorMessage);
        if (!run.has_value())
        {
            return std::nullopt;
        }
        runs.push_back(run.value());
    }
    return runs;
}

void PersistenceWorker::claimNext(const QString& runId)
{
    if (!ensureReady(QStringLiteral("claim_next"), runId))
    {
        return;
    }

    QString errorMessage;
    QSqlDatabase database = dataBase();
    const auto item = InspectionItemRepository::claimNextPending(
        database,
        runId,
        PersistenceWorkerUtils::currentTimeMs(),
        &errorMessage);
    if (!item.has_value())
    {
        emit operationFailed(
            QStringLiteral("claim_next"),
            runId,
            errorMessage);
        return;
    }
    emit itemClaimed(item.value());
}

void PersistenceWorker::persistSuccess(
    const ClaimedInspectionItem& item,
    const InferenceResult& result)
{
    if (!ensureReady(QStringLiteral("persist_success"), item.resultId))
    {
        return;
    }
    if (!item.found || result.heatmapOverlay.isNull())
    {
        emit operationFailed(
            QStringLiteral("persist_success"),
            item.resultId,
            QStringLiteral("Successful Result or Heatmap is Invalid."));
        return;
    }

    QString errorMessage;
    QSqlDatabase database = dataBase();
    const auto run = readRun(database, item.runId, &errorMessage);
    if (!run.has_value())
    {
        emit operationFailed(
            QStringLiteral("persist_success"),
            item.resultId,
            errorMessage);
        return;
    }

    const QString relativePath =
        QStringLiteral("heatmaps/%1.png").arg(item.resultId);
    const QString absolutePath =
        QDir(run->storagePath).filePath(relativePath);
    if (!isManagedRunPath(absolutePath))
    {
        emit operationFailed(
            QStringLiteral("persist_success"),
            item.resultId,
            QStringLiteral("Heatmap Path is Outside the Managed Root."));
        return;
    }

    QSaveFile heatmapFile(absolutePath);
    if (!heatmapFile.open(QIODevice::WriteOnly)
        || !result.heatmapOverlay.save(&heatmapFile, "PNG")
        || !heatmapFile.commit())
    {
        heatmapFile.cancelWriting();
        emit operationFailed(
            QStringLiteral("persist_success"),
            item.resultId,
            QStringLiteral("Failed to Atomically Save Heatmap PNG: %1")
                .arg(heatmapFile.errorString()));
        return;
    }

    SuccessfulInspectionResult storedResult;
    storedResult.resultId = item.resultId;
    storedResult.runId = item.runId;
    storedResult.detectionLevel =
        PersistenceWorkerUtils::detectionLevelText(result.detectionLevel);
    storedResult.score = result.score;
    storedResult.inferenceMs = result.inferenceMs;
    storedResult.defectArea = result.defectArea;
    storedResult.heatmapRelativePath = relativePath;
    storedResult.completedAtMs = PersistenceWorkerUtils::currentTimeMs();

    if (!InspectionItemRepository::completeSuccess(
            database,
            storedResult,
            &errorMessage))
    {
        QFile::remove(absolutePath);
        emit operationFailed(
            QStringLiteral("persist_success"),
            item.resultId,
            errorMessage);
        return;
    }

    PersistedInspectionResult persisted;
    persisted.resultId = item.resultId;
    persisted.runId = item.runId;
    persisted.sequenceNo = item.sequenceNo;
    persisted.sourceFileName = item.sourceFileName;
    persisted.sourceFilePath = item.sourceFilePath;
    persisted.detectionLevel = storedResult.detectionLevel;
    persisted.score = storedResult.score;
    persisted.inferenceMs = storedResult.inferenceMs;
    persisted.defectArea = storedResult.defectArea;
    persisted.heatmapFilePath = absolutePath;
    emit successPersisted(persisted);
}

void PersistenceWorker::persistFailure(
    const ClaimedInspectionItem& item,
    const QString& errorMessage)
{
    if (!ensureReady(QStringLiteral("persist_failure"), item.resultId))
    {
        return;
    }

    FailedInspectionResult result;
    result.resultId = item.resultId;
    result.runId = item.runId;
    result.errorMessage =
        errorMessage.trimmed().isEmpty()
            ? QStringLiteral("Unknown Image Processing Failure.")
            : errorMessage.trimmed();
    result.completedAtMs = PersistenceWorkerUtils::currentTimeMs();

    QString repositoryError;
    QSqlDatabase database = dataBase();
    if (!InspectionItemRepository::completeFailure(
            database,
            result,
            &repositoryError))
    {
        emit operationFailed(
            QStringLiteral("persist_failure"),
            item.resultId,
            repositoryError);
        return;
    }

    InspectionItemPageRow row;
    row.resultId = item.resultId;
    row.sequenceNo = item.sequenceNo;
    row.sourceFileName = item.sourceFileName;
    row.sourceFilePath = item.sourceFilePath;
    row.status = QStringLiteral("FAILED");
    row.errorMessage = result.errorMessage;
    emit failurePersisted(row);
}

void PersistenceWorker::finishAsDraft(const QString& runId)
{
    if (!ensureReady(QStringLiteral("finish_as_draft"), runId))
    {
        return;
    }

    QString errorMessage;
    QSqlDatabase database = dataBase();
    if (!InspectionRunRepository::finishAsDraft(
            database,
            runId,
            PersistenceWorkerUtils::currentTimeMs(),
            &errorMessage))
    {
        emit operationFailed(
            QStringLiteral("finish_as_draft"),
            runId,
            errorMessage);
        return;
    }

    const auto run = readRun(database, runId, &errorMessage);
    if (!run.has_value())
    {
        emit operationFailed(
            QStringLiteral("read_draft"),
            runId,
            errorMessage);
        return;
    }
    emit runDrafted(run.value());
}

bool PersistenceWorker::writeRunManifest(
    const QSqlDatabase& database,
    const InspectionRunRecord& run,
    SaveInspectionRunRequest* request,
    QString* errorMessage) const
{
    if (request == nullptr || !isManagedRunPath(run.storagePath))
    {
        if (errorMessage != nullptr)
        {
            *errorMessage = QStringLiteral("Manifest Target is Invalid.");
        }
        return false;
    }

    const QString itemsRelativePath = QStringLiteral("items.json");
    const QString manifestRelativePath = QStringLiteral("manifest.json");
    const QString itemsPath =
        QDir(run.storagePath).filePath(itemsRelativePath);
    const QString manifestPath =
        QDir(run.storagePath).filePath(manifestRelativePath);

    QSaveFile itemsFile(itemsPath);
    if (!itemsFile.open(QIODevice::WriteOnly))
    {
        if (errorMessage != nullptr)
        {
            *errorMessage = QStringLiteral("Failed to Open Items Manifest: %1")
                                .arg(itemsFile.errorString());
        }
        return false;
    }

    QCryptographicHash itemsHash(QCryptographicHash::Sha256);
    if (!PersistenceWorkerUtils::writeAndHash(
            itemsFile,
            itemsHash,
            QByteArrayLiteral("["),
            errorMessage))
    {
        itemsFile.cancelWriting();
        return false;
    }

    QSqlQuery query(database);
    query.prepare(
        QStringLiteral(
            "SELECT result_id, sequence_no, source_file_name, "
            "source_file_path, status, detection_level, score, "
            "inference_ms, defect_area, heatmap_rel_path, error_message "
            "FROM inspection_item WHERE run_id = :run_id "
            "ORDER BY sequence_no ASC;"));
    query.bindValue(QStringLiteral(":run_id"), run.runId);
    if (!query.exec())
    {
        itemsFile.cancelWriting();
        if (errorMessage != nullptr)
        {
            *errorMessage = QStringLiteral("Failed to Query Manifest Items: %1")
                                .arg(SqliteRepositoryUtils::formatSqlError(
                                    query.lastError()));
        }
        return false;
    }

    bool first = true;
    while (query.next())
    {
        QByteArray bytes =
            QJsonDocument(PersistenceWorkerUtils::itemJsonObject(query))
                .toJson(QJsonDocument::Compact);
        if (!first)
        {
            bytes.prepend(',');
        }
        first = false;
        if (!PersistenceWorkerUtils::writeAndHash(
                itemsFile,
                itemsHash,
                bytes,
                errorMessage))
        {
            itemsFile.cancelWriting();
            return false;
        }
    }
    if (query.lastError().isValid())
    {
        itemsFile.cancelWriting();
        if (errorMessage != nullptr)
        {
            *errorMessage = QStringLiteral("Failed While Reading Items: %1")
                                .arg(SqliteRepositoryUtils::formatSqlError(
                                    query.lastError()));
        }
        return false;
    }

    if (!PersistenceWorkerUtils::writeAndHash(
            itemsFile,
            itemsHash,
            QByteArrayLiteral("]"),
            errorMessage)
        || !itemsFile.commit())
    {
        itemsFile.cancelWriting();
        if (errorMessage != nullptr && errorMessage->isEmpty())
        {
            *errorMessage = QStringLiteral("Failed to Commit Items Manifest: %1")
                                .arg(itemsFile.errorString());
        }
        return false;
    }

    const QString itemsSha =
        QString::fromLatin1(itemsHash.result().toHex());
    QJsonObject manifest;
    manifest.insert(QStringLiteral("manifest_version"), 1);
    manifest.insert(QStringLiteral("run_id"), run.runId);
    manifest.insert(QStringLiteral("name"), request->name.trimmed());
    if (request->note.has_value())
    {
        manifest.insert(QStringLiteral("note"), request->note.value());
    }
    manifest.insert(QStringLiteral("status"), QStringLiteral("SAVED"));
    manifest.insert(QStringLiteral("source_type"), run.sourceType);
    manifest.insert(QStringLiteral("source_path"), run.sourcePath);
    manifest.insert(QStringLiteral("model_path"), run.modelPath);
    manifest.insert(QStringLiteral("model_format"), run.modelFormat);
    manifest.insert(QStringLiteral("model_sha256"), run.modelHash);
    manifest.insert(
        QStringLiteral("review_threshold"),
        run.reviewThreshold);
    manifest.insert(QStringLiteral("ng_threshold"), run.ngThreshold);
    manifest.insert(QStringLiteral("heatmap_alpha"), run.heatmapAlpha);
    manifest.insert(QStringLiteral("started_at_ms"), run.startedAtMs);
    manifest.insert(QStringLiteral("total_count"), run.totalCount);
    manifest.insert(
        QStringLiteral("completed_count"),
        run.completedCount);
    manifest.insert(QStringLiteral("failed_count"), run.failedCount);
    manifest.insert(QStringLiteral("ok_count"), run.okCount);
    manifest.insert(QStringLiteral("review_count"), run.reviewCount);
    manifest.insert(QStringLiteral("ng_count"), run.ngCount);
    manifest.insert(QStringLiteral("items_file"), itemsRelativePath);
    manifest.insert(QStringLiteral("items_sha256"), itemsSha);

    const QByteArray manifestBytes =
        QJsonDocument(manifest).toJson(QJsonDocument::Indented);
    QSaveFile manifestFile(manifestPath);
    if (!manifestFile.open(QIODevice::WriteOnly)
        || manifestFile.write(manifestBytes) != manifestBytes.size()
        || !manifestFile.commit())
    {
        manifestFile.cancelWriting();
        if (errorMessage != nullptr)
        {
            *errorMessage =
                QStringLiteral("Failed to Atomically Write Manifest: %1")
                    .arg(manifestFile.errorString());
        }
        return false;
    }

    request->manifestVersion = 1;
    request->manifestRelativePath = manifestRelativePath;
    request->manifestSha256 =
        QString::fromLatin1(
            QCryptographicHash::hash(
                manifestBytes,
                QCryptographicHash::Sha256)
                .toHex());
    request->itemsRelativePath = itemsRelativePath;
    request->itemsSha256 = itemsSha;
    return true;
}

void PersistenceWorker::saveDraft(
    const QString& runId,
    const QString& name,
    const QString& note)
{
    if (!ensureReady(QStringLiteral("save_draft"), runId))
    {
        return;
    }

    QString errorMessage;
    QSqlDatabase database = dataBase();
    const auto run = readRun(database, runId, &errorMessage);
    if (!run.has_value())
    {
        emit operationFailed(
            QStringLiteral("save_draft"),
            runId,
            errorMessage);
        return;
    }

    SaveInspectionRunRequest request;
    request.runId = runId;
    request.name = name.trimmed();
    if (!note.trimmed().isEmpty())
    {
        request.note = note.trimmed();
    }
    request.savedAtMs = PersistenceWorkerUtils::currentTimeMs();

    if (!writeRunManifest(
            database,
            run.value(),
            &request,
            &errorMessage)
        || !InspectionRunRepository::saveDraft(
            database,
            request,
            &errorMessage))
    {
        emit operationFailed(
            QStringLiteral("save_draft"),
            runId,
            errorMessage);
        return;
    }

    emit runSaved(runId);
}

void PersistenceWorker::discardDraft(const QString& runId)
{
    if (!ensureReady(QStringLiteral("discard_draft"), runId))
    {
        return;
    }

    QString errorMessage;
    QSqlDatabase database = dataBase();
    if (!InspectionRunRepository::discardDraft(
            database,
            runId,
            PersistenceWorkerUtils::currentTimeMs(),
            &errorMessage))
    {
        emit operationFailed(
            QStringLiteral("discard_draft"),
            runId,
            errorMessage);
        return;
    }

    processCleanupJobs(database);
    emit runDiscarded(runId);
}

void PersistenceWorker::requestBatchCatalog()
{
    if (!ensureReady(QStringLiteral("batch_catalog"), {}))
    {
        return;
    }

    QString errorMessage;
    QSqlDatabase database = dataBase();
    const auto catalog = readBatchCatalog(database, &errorMessage);
    if (!catalog.has_value())
    {
        emit operationFailed(
            QStringLiteral("batch_catalog"),
            {},
            errorMessage);
        return;
    }
    emit batchCatalogReady(catalog.value());
}

void PersistenceWorker::loadSavedRun(const QString& runId)
{
    if (!ensureReady(QStringLiteral("load_saved_run"), runId))
    {
        return;
    }

    QString errorMessage;
    QSqlDatabase database = dataBase();
    const auto run = readRun(database, runId, &errorMessage);
    if (!run.has_value() || run->status != QStringLiteral("SAVED"))
    {
        emit operationFailed(
            QStringLiteral("load_saved_run"),
            runId,
            run.has_value()
                ? QStringLiteral("Only SAVED Runs Can be Loaded for Viewing.")
                : errorMessage);
        return;
    }

    const auto items = readAllItems(database, runId, &errorMessage);
    if (!items.has_value())
    {
        emit operationFailed(
            QStringLiteral("load_saved_run"),
            runId,
            errorMessage);
        return;
    }
    emit runLoaded(run.value(), items.value(), false);
}

void PersistenceWorker::resumeInterruptedRun(const QString& runId)
{
    if (!ensureReady(QStringLiteral("resume_run"), runId))
    {
        return;
    }

    QString errorMessage;
    QSqlDatabase database = dataBase();
    if (!InspectionRunRepository::resumeInterruptedRun(
            database,
            runId,
            PersistenceWorkerUtils::currentTimeMs(),
            &errorMessage))
    {
        emit operationFailed(
            QStringLiteral("resume_run"),
            runId,
            errorMessage);
        return;
    }

    const auto run = readRun(database, runId, &errorMessage);
    const auto items =
        run.has_value()
            ? readAllItems(database, runId, &errorMessage)
            : std::nullopt;
    if (!run.has_value() || !items.has_value())
    {
        (void)interruptRunInDataBase(
            database,
            runId,
            PersistenceWorkerUtils::currentTimeMs(),
            nullptr);
        emit operationFailed(
            QStringLiteral("resume_run"),
            runId,
            errorMessage);
        return;
    }
    emit runLoaded(run.value(), items.value(), true);
}

void PersistenceWorker::deleteSavedRun(const QString& runId)
{
    if (!ensureReady(QStringLiteral("delete_saved_run"), runId))
    {
        return;
    }

    QString errorMessage;
    QSqlDatabase database = dataBase();
    if (!InspectionRunRepository::deleteSaved(
            database,
            runId,
            PersistenceWorkerUtils::currentTimeMs(),
            &errorMessage))
    {
        emit operationFailed(
            QStringLiteral("delete_saved_run"),
            runId,
            errorMessage);
        return;
    }

    processCleanupJobs(database);
    emit savedRunDeleted(runId);
}

bool PersistenceWorker::interruptRunInDataBase(
    QSqlDatabase& database,
    const QString& runId,
    qint64 interruptedAtMs,
    QString* errorMessage)
{
    SqliteImmediateTransaction transaction(database);
    QString transactionError;
    if (!transaction.begin(&transactionError))
    {
        if (errorMessage != nullptr)
        {
            *errorMessage = transactionError;
        }
        return false;
    }

    QSqlQuery resetQuery(database);
    resetQuery.prepare(
        QStringLiteral(
            "UPDATE inspection_item "
            "SET status = 'PENDING', processing_started_at_ms = NULL, "
            "updated_at_ms = :time "
            "WHERE run_id = :run_id AND status = 'PROCESSING';"));
    resetQuery.bindValue(QStringLiteral(":time"), interruptedAtMs);
    resetQuery.bindValue(QStringLiteral(":run_id"), runId);
    if (!resetQuery.exec())
    {
        (void)transaction.rollback();
        if (errorMessage != nullptr)
        {
            *errorMessage = QStringLiteral("Failed to Reset Active Item: %1")
                                .arg(SqliteRepositoryUtils::formatSqlError(
                                    resetQuery.lastError()));
        }
        return false;
    }

    QSqlQuery runQuery(database);
    runQuery.prepare(
        QStringLiteral(
            "UPDATE inspection_run "
            "SET status = 'INTERRUPTED', "
            "last_interrupted_at_ms = :time, updated_at_ms = :time, "
            "revision = revision + 1 "
            "WHERE run_id = :run_id AND status = 'RUNNING';"));
    runQuery.bindValue(QStringLiteral(":time"), interruptedAtMs);
    runQuery.bindValue(QStringLiteral(":run_id"), runId);
    if (!runQuery.exec())
    {
        (void)transaction.rollback();
        if (errorMessage != nullptr)
        {
            *errorMessage = QStringLiteral("Failed to Interrupt Run: %1")
                                .arg(SqliteRepositoryUtils::formatSqlError(
                                    runQuery.lastError()));
        }
        return false;
    }

    if (runQuery.numRowsAffected() != 1)
    {
        QSqlQuery statusQuery(database);
        statusQuery.prepare(
            QStringLiteral(
                "SELECT status FROM inspection_run WHERE run_id = :run_id;"));
        statusQuery.bindValue(QStringLiteral(":run_id"), runId);
        const bool alreadyInterrupted =
            statusQuery.exec()
            && statusQuery.next()
            && statusQuery.value(0).toString()
                   == QStringLiteral("INTERRUPTED");
        if (!alreadyInterrupted)
        {
            (void)transaction.rollback();
            if (errorMessage != nullptr)
            {
                *errorMessage =
                    QStringLiteral("Run is not in RUNNING State.");
            }
            return false;
        }
    }

    if (!transaction.commit(&transactionError))
    {
        if (errorMessage != nullptr)
        {
            *errorMessage = transactionError;
        }
        return false;
    }
    return true;
}

void PersistenceWorker::interruptRunningRun(const QString& runId)
{
    if (!ensureReady(QStringLiteral("interrupt_run"), runId))
    {
        return;
    }

    QString errorMessage;
    QSqlDatabase database = dataBase();
    if (!interruptRunInDataBase(
            database,
            runId,
            PersistenceWorkerUtils::currentTimeMs(),
            &errorMessage))
    {
        emit operationFailed(
            QStringLiteral("interrupt_run"),
            runId,
            errorMessage);
        return;
    }
    emit runInterrupted(runId);
}

bool PersistenceWorker::isManagedRunPath(const QString& path) const
{
    const QString root = QDir::fromNativeSeparators(
        QDir::cleanPath(
            QFileInfo(managedRunsRoot_).absoluteFilePath()));
    const QString candidate =
        QDir::fromNativeSeparators(
            QDir::cleanPath(QFileInfo(path).absoluteFilePath()));
    if (candidate == root)
    {
        return false;
    }

    const QString prefix = root + QLatin1Char('/');
    return candidate.startsWith(prefix, Qt::CaseInsensitive);
}

void PersistenceWorker::discardStartupDrafts(QSqlDatabase& database)
{
    QSqlQuery query(database);
    if (!query.exec(
            QStringLiteral(
                "SELECT run_id FROM inspection_run "
                "WHERE status = 'DRAFT' ORDER BY started_at_ms ASC;")))
    {
        emit operationFailed(
            QStringLiteral("discard_startup_drafts"),
            {},
            SqliteRepositoryUtils::formatSqlError(query.lastError()));
        return;
    }

    QVector<QString> runIds;
    while (query.next())
    {
        runIds.push_back(query.value(0).toString());
    }
    query.finish();

    for (const QString& runId : runIds)
    {
        QString errorMessage;
        if (!InspectionRunRepository::discardDraft(
                database,
                runId,
                PersistenceWorkerUtils::currentTimeMs(),
                &errorMessage))
        {
            emit operationFailed(
                QStringLiteral("discard_startup_draft"),
                runId,
                errorMessage);
        }
    }
}

void PersistenceWorker::processCleanupJobs(QSqlDatabase& database)
{
    QSqlQuery query(database);
    if (!query.exec(
            QStringLiteral(
                "SELECT job_id, target_path, target_kind "
                "FROM storage_cleanup_job "
                "ORDER BY created_at_ms ASC, job_id ASC LIMIT 100;")))
    {
        emit operationFailed(
            QStringLiteral("process_cleanup_jobs"),
            {},
            SqliteRepositoryUtils::formatSqlError(query.lastError()));
        return;
    }

    struct CleanupJob
    {
        qint64 jobId = 0;
        QString targetPath;
        QString targetKind;
    };
    QVector<CleanupJob> jobs;
    while (query.next())
    {
        CleanupJob job;
        job.jobId = query.value(0).toLongLong();
        job.targetPath = query.value(1).toString();
        job.targetKind = query.value(2).toString();
        jobs.push_back(job);
    }
    query.finish();

    for (const CleanupJob& job : jobs)
    {
        bool removed = false;
        QString failure;
        if (!isManagedRunPath(job.targetPath))
        {
            failure = QStringLiteral(
                "Cleanup Target is Outside the Managed Runs Root.");
        }
        else if (!QFileInfo::exists(job.targetPath))
        {
            removed = true;
        }
        else if (job.targetKind == QStringLiteral("DIRECTORY"))
        {
            removed = QDir(job.targetPath).removeRecursively();
            if (!removed)
            {
                failure = QStringLiteral("Failed to Remove Directory.");
            }
        }
        else
        {
            removed = QFile::remove(job.targetPath);
            if (!removed)
            {
                failure = QStringLiteral("Failed to Remove File.");
            }
        }

        QSqlQuery update(database);
        if (removed)
        {
            update.prepare(
                QStringLiteral(
                    "DELETE FROM storage_cleanup_job WHERE job_id = :job_id;"));
            update.bindValue(QStringLiteral(":job_id"), job.jobId);
        }
        else
        {
            update.prepare(
                QStringLiteral(
                    "UPDATE storage_cleanup_job "
                    "SET attempt_count = attempt_count + 1, "
                    "last_attempt_at_ms = :time, last_error = :error "
                    "WHERE job_id = :job_id;"));
            update.bindValue(
                QStringLiteral(":time"),
                PersistenceWorkerUtils::currentTimeMs());
            update.bindValue(QStringLiteral(":error"), failure);
            update.bindValue(QStringLiteral(":job_id"), job.jobId);
        }

        if (!update.exec())
        {
            emit operationFailed(
                QStringLiteral("update_cleanup_job"),
                QString::number(job.jobId),
                SqliteRepositoryUtils::formatSqlError(update.lastError()));
        }
    }
}
