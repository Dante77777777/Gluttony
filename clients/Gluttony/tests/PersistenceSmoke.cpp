#include "DataBase/DataBaseConnection.h"
#include "DataBase/DatabaseBootstrapper.h"
#include "DataBase/InspectionItemRepository.h"
#include "DataBase/InspectionRunRepository.h"
#include "DataBase/MigrationCatalog.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTextStream>
#include <QUuid>

namespace
{

bool check(bool condition, const QString& message)
{
    if (condition)
    {
        return true;
    }

    QTextStream(stderr) << "FAILED: " << message << Qt::endl;
    return false;
}

QString newUuid()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

bool verifyFailedMigrationRollback(
    const QString& databasePath,
    const QVector<MigrationSource>& migrations)
{
    QVector<MigrationSource> brokenMigrations = migrations;
    brokenMigrations[0].statements[5] =
        QByteArrayLiteral("THIS IS NOT VALID SQLITE;");

    QString bootstrapError;
    const auto report = DatabaseBootstrapper::initialize(
        databasePath,
        QStringLiteral("smoke.rollback.bootstrap"),
        brokenMigrations,
        &bootstrapError);

    if (!check(
            !report.has_value(),
            QStringLiteral("Broken Migration Unexpectedly Succeeded.")))
    {
        return false;
    }

    QString connectionError;
    auto connection = DataBaseConnection::openSqlite(
        databasePath,
        QStringLiteral("smoke.rollback.verify"),
        &connectionError);

    if (!check(
            connection.has_value(),
            QStringLiteral("Rollback Verification Connection Failed: %1")
                .arg(connectionError)))
    {
        return false;
    }

    bool result = true;
    {
        QSqlDatabase database =
            QSqlDatabase::database(QStringLiteral("smoke.rollback.verify"), false);

        QSqlQuery versionQuery(database);
        result = check(
                     versionQuery.exec(QStringLiteral("PRAGMA user_version;"))
                         && versionQuery.next()
                         && versionQuery.value(0).toInt() == 0,
                     QStringLiteral("Failed Migration Changed user_version."))
                 && result;
        versionQuery.finish();

        QSqlQuery tableQuery(database);
        result = check(
                     tableQuery.exec(
                         QStringLiteral(
                             "SELECT COUNT(*) FROM sqlite_schema "
                             "WHERE type = 'table' "
                             "AND name = 'schema_migration';"))
                         && tableQuery.next()
                         && tableQuery.value(0).toInt() == 0,
                     QStringLiteral(
                         "Failed Migration Left schema_migration Behind."))
                 && result;
        tableQuery.finish();
    }

    connection.reset();
    return result;
}

bool exerciseRepositories(
    const QString& databasePath,
    const QVector<MigrationSource>& migrations)
{
    QString bootstrapError;
    const auto firstBootstrap = DatabaseBootstrapper::initialize(
        databasePath,
        QStringLiteral("smoke.bootstrap.first"),
        migrations,
        &bootstrapError);

    if (!check(
            firstBootstrap.has_value(),
            QStringLiteral("First Bootstrap Failed: %1").arg(bootstrapError))
        || !check(
            firstBootstrap->initialUserVersion == 0
                && firstBootstrap->finalUserVersion == 1
                && firstBootstrap->applicationId == 1196183374
                && firstBootstrap->appliedThisStartupCount == 1,
            QStringLiteral("First Bootstrap Report is Incorrect.")))
    {
        return false;
    }

    bootstrapError.clear();
    const auto secondBootstrap = DatabaseBootstrapper::initialize(
        databasePath,
        QStringLiteral("smoke.bootstrap.second"),
        migrations,
        &bootstrapError);

    if (!check(
            secondBootstrap.has_value(),
            QStringLiteral("Second Bootstrap Failed: %1").arg(bootstrapError))
        || !check(
            secondBootstrap->initialUserVersion == 1
                && secondBootstrap->finalUserVersion == 1
                && secondBootstrap->appliedThisStartupCount == 0,
            QStringLiteral("Second Bootstrap was not Idempotent.")))
    {
        return false;
    }

    QString connectionError;
    auto connection = DataBaseConnection::openSqlite(
        databasePath,
        QStringLiteral("smoke.repository"),
        &connectionError);

    if (!check(
            connection.has_value(),
            QStringLiteral("Repository Connection Failed: %1")
                .arg(connectionError)))
    {
        return false;
    }

    bool result = true;
    {
        QSqlDatabase database =
            QSqlDatabase::database(QStringLiteral("smoke.repository"), false);

        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        const QString runId = newUuid();

        NewInspectionRun run;
        run.runId = runId;
        run.sourceType = QStringLiteral("FOLDER");
        run.sourcePath = QStringLiteral("D:/smoke/images");
        run.storagePath = QStringLiteral("D:/smoke/runs/%1").arg(runId);
        run.storageOrigin = QStringLiteral("MANAGED");
        run.modelPath = QStringLiteral("D:/smoke/model.onnx");
        run.modelFormat = QStringLiteral("ONNX");
        run.modelHash = QString(64, QLatin1Char('a'));
        run.appVersion = QStringLiteral("smoke-test");
        run.reviewThreshold = 0.45;
        run.ngThreshold = 0.60;
        run.heatmapAlpha = 0.55;
        run.startedAtMs = now;

        QVector<NewInspectionItem> items;
        for (int sequence = 1; sequence <= 2; ++sequence)
        {
            NewInspectionItem item;
            item.resultId = newUuid();
            item.sequenceNo = sequence;
            item.sourceFileName =
                QStringLiteral("%1.png").arg(sequence, 3, 10, QLatin1Char('0'));
            item.sourceFilePath =
                QStringLiteral("D:/smoke/images/%1").arg(item.sourceFileName);
            item.sourceFileSize = 1024 + sequence;
            item.sourceModifiedAtMs = now;
            item.createdAtMs = now;
            items.push_back(item);
        }

        QString repositoryError;
        result = check(
                     InspectionRunRepository::createWithInputItems(
                         database,
                         run,
                         items,
                         &repositoryError),
                     QStringLiteral("Run Creation Failed: %1")
                         .arg(repositoryError))
                 && result;

        repositoryError.clear();
        const auto firstClaim = InspectionItemRepository::claimNextPending(
            database,
            runId,
            now + 1,
            &repositoryError);
        result = check(
                     firstClaim.has_value()
                         && firstClaim->found
                         && firstClaim->sequenceNo == 1,
                     QStringLiteral("First Claim Failed: %1")
                         .arg(repositoryError))
                 && result;

        if (firstClaim.has_value() && firstClaim->found)
        {
            SuccessfulInspectionResult success;
            success.resultId = firstClaim->resultId;
            success.runId = runId;
            success.detectionLevel = QStringLiteral("NG");
            success.score = 0.75;
            success.inferenceMs = 42;
            success.defectArea = 100;
            success.heatmapRelativePath =
                QStringLiteral("heatmaps/%1.png").arg(success.resultId);
            success.completedAtMs = now + 2;

            repositoryError.clear();
            result = check(
                         InspectionItemRepository::completeSuccess(
                             database,
                             success,
                             &repositoryError),
                         QStringLiteral("Success Persistence Failed: %1")
                             .arg(repositoryError))
                     && result;
        }

        repositoryError.clear();
        const auto secondClaim = InspectionItemRepository::claimNextPending(
            database,
            runId,
            now + 3,
            &repositoryError);
        result = check(
                     secondClaim.has_value()
                         && secondClaim->found
                         && secondClaim->sequenceNo == 2,
                     QStringLiteral("Second Claim Failed: %1")
                         .arg(repositoryError))
                 && result;

        if (secondClaim.has_value() && secondClaim->found)
        {
            FailedInspectionResult failure;
            failure.resultId = secondClaim->resultId;
            failure.runId = runId;
            failure.errorMessage = QStringLiteral("Synthetic smoke failure");
            failure.completedAtMs = now + 4;

            repositoryError.clear();
            result = check(
                         InspectionItemRepository::completeFailure(
                             database,
                             failure,
                             &repositoryError),
                         QStringLiteral("Failure Persistence Failed: %1")
                             .arg(repositoryError))
                     && result;
        }

        repositoryError.clear();
        result = check(
                     InspectionRunRepository::finishAsDraft(
                         database,
                         runId,
                         now + 5,
                         &repositoryError),
                     QStringLiteral("Finish as Draft Failed: %1")
                         .arg(repositoryError))
                 && result;

        SaveInspectionRunRequest saveRequest;
        saveRequest.runId = runId;
        saveRequest.name = QStringLiteral("Smoke Batch");
        saveRequest.note = QStringLiteral("Persistence smoke test");
        saveRequest.savedAtMs = now + 6;
        saveRequest.manifestVersion = 1;
        saveRequest.manifestRelativePath = QStringLiteral("batch.json");
        saveRequest.manifestSha256 = QString(64, QLatin1Char('b'));
        saveRequest.itemsRelativePath = QStringLiteral("items.jsonl");
        saveRequest.itemsSha256 = QString(64, QLatin1Char('c'));

        repositoryError.clear();
        result = check(
                     InspectionRunRepository::saveDraft(
                         database,
                         saveRequest,
                         &repositoryError),
                     QStringLiteral("Run Save Failed: %1")
                         .arg(repositoryError))
                 && result;

        repositoryError.clear();
        result = check(
                     InspectionRunRepository::saveDraft(
                         database,
                         saveRequest,
                         &repositoryError),
                     QStringLiteral("Idempotent Save Failed: %1")
                         .arg(repositoryError))
                 && result;

        repositoryError.clear();
        const auto history = InspectionRunRepository::listSavedHistory(
            database,
            50,
            std::nullopt,
            QString(),
            &repositoryError);
        result = check(
                     history.has_value()
                         && history->size() == 1
                         && history->first().runId == runId,
                     QStringLiteral("History Query Failed: %1")
                         .arg(repositoryError))
                 && result;

        repositoryError.clear();
        const auto itemPage = InspectionItemRepository::listPage(
            database,
            runId,
            0,
            100,
            &repositoryError);
        result = check(
                     itemPage.has_value()
                         && itemPage->size() == 2
                         && itemPage->at(0).status == QStringLiteral("SUCCESS")
                         && itemPage->at(1).status == QStringLiteral("FAILED"),
                     QStringLiteral("Item Page Query Failed: %1")
                         .arg(repositoryError))
                 && result;

        repositoryError.clear();
        result = check(
                     InspectionRunRepository::deleteSaved(
                         database,
                         runId,
                         now + 7,
                         &repositoryError),
                     QStringLiteral("Saved Run Delete Failed: %1")
                         .arg(repositoryError))
                 && result;

        QSqlQuery deletionQuery(database);
        deletionQuery.prepare(
            QStringLiteral(
                "SELECT "
                "(SELECT COUNT(*) FROM inspection_run WHERE run_id = :run_id), "
                "(SELECT COUNT(*) FROM inspection_item WHERE run_id = :run_id), "
                "(SELECT COUNT(*) FROM storage_cleanup_job "
                " WHERE origin_run_id = :run_id);"));
        deletionQuery.bindValue(QStringLiteral(":run_id"), runId);
        result = check(
                     deletionQuery.exec()
                         && deletionQuery.next()
                         && deletionQuery.value(0).toInt() == 0
                         && deletionQuery.value(1).toInt() == 0
                         && deletionQuery.value(2).toInt() == 1,
                     QStringLiteral(
                         "Saved Run Delete/Cascade/Cleanup Verification Failed."))
                 && result;
        deletionQuery.finish();

        const QString recoveryRunId = newUuid();
        run.runId = recoveryRunId;
        run.storagePath =
            QStringLiteral("D:/smoke/runs/%1").arg(recoveryRunId);
        run.startedAtMs = now + 10;

        QVector<NewInspectionItem> recoveryItems;
        NewInspectionItem recoveryItem;
        recoveryItem.resultId = newUuid();
        recoveryItem.sequenceNo = 1;
        recoveryItem.sourceFileName = QStringLiteral("recovery.png");
        recoveryItem.sourceFilePath =
            QStringLiteral("D:/smoke/images/recovery.png");
        recoveryItem.sourceFileSize = 2048;
        recoveryItem.sourceModifiedAtMs = now + 10;
        recoveryItem.createdAtMs = now + 10;
        recoveryItems.push_back(recoveryItem);

        repositoryError.clear();
        result = check(
                     InspectionRunRepository::createWithInputItems(
                         database,
                         run,
                         recoveryItems,
                         &repositoryError),
                     QStringLiteral("Recovery Run Creation Failed: %1")
                         .arg(repositoryError))
                 && result;

        repositoryError.clear();
        const auto recoveryClaim =
            InspectionItemRepository::claimNextPending(
                database,
                recoveryRunId,
                now + 11,
                &repositoryError);
        result = check(
                     recoveryClaim.has_value() && recoveryClaim->found,
                     QStringLiteral("Recovery Claim Failed: %1")
                         .arg(repositoryError))
                 && result;

        repositoryError.clear();
        const auto recoveryReport =
            InspectionRunRepository::recoverInterruptedRuns(
                database,
                now + 12,
                &repositoryError);
        result = check(
                     recoveryReport.has_value()
                         && recoveryReport->resetItemCount == 1
                         && recoveryReport->interruptedRunCount == 1,
                     QStringLiteral("Startup Recovery Failed: %1")
                         .arg(repositoryError))
                 && result;

        repositoryError.clear();
        result = check(
                     InspectionRunRepository::resumeInterruptedRun(
                         database,
                         recoveryRunId,
                         now + 13,
                         &repositoryError),
                     QStringLiteral("Recovery Resume Failed: %1")
                         .arg(repositoryError))
                 && result;
    }

    connection.reset();
    result = check(
                 !QSqlDatabase::contains(QStringLiteral("smoke.repository")),
                 QStringLiteral("Repository Connection was not Removed."))
             && result;
    return result;
}

}

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);
    QCoreApplication::setApplicationName(
        QStringLiteral("GluttonyPersistenceSmoke"));

    QTemporaryDir temporaryDirectory;
    if (!check(
            temporaryDirectory.isValid(),
            QStringLiteral("Could not Create Temporary Directory.")))
    {
        return 1;
    }

    QString migrationError;
    const auto migrations =
        MigrationCatalog::loadEmbeddedMigrations(&migrationError);

    if (!check(
            migrations.has_value(),
            QStringLiteral("Migration Catalog Failed: %1")
                .arg(migrationError)))
    {
        return 1;
    }

    const QString rollbackDatabase =
        QDir(temporaryDirectory.path())
            .filePath(QStringLiteral("rollback.db"));
    const QString repositoryDatabase =
        QDir(temporaryDirectory.path())
            .filePath(QStringLiteral("repository.db"));

    if (!verifyFailedMigrationRollback(
            rollbackDatabase,
            migrations.value()))
    {
        return 1;
    }

    if (!exerciseRepositories(
            repositoryDatabase,
            migrations.value()))
    {
        return 1;
    }

    QTextStream(stdout)
        << "Gluttony persistence smoke test passed."
        << Qt::endl;
    return 0;
}
