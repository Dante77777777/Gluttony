#include "DatabaseBootstrapper.h"

#include "DataBaseConnection.h"
#include "InspectionRunRepository.h"
#include "MigrationPlanner.h"
#include "SqliteTransaction.h"

#include <QDateTime>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QVariant>

namespace DatabaseBootstrapperUtils
{

constexpr int kGluttonyApplicationId = 1196183374;

void setErrorMessage(QString* errorMessage, const QString& message)
{
    if (errorMessage != nullptr)
    {
        *errorMessage = message;
    }
}

bool readIntegerPragma(
    const QSqlDatabase& database,
    const QString& statement,
    int* value,
    QString* errorMessage)
{
    QSqlQuery query(database);
    if (!query.exec(statement))
    {
        setErrorMessage(
            errorMessage,
            QStringLiteral("Failed to Read '%1': %2")
                .arg(statement)
                .arg(SqliteRepositoryUtils::formatSqlError(
                    query.lastError())));
        return false;
    }

    if (!query.next())
    {
        query.finish();
        setErrorMessage(
            errorMessage,
            QStringLiteral("PRAGMA Returned no Rows: %1").arg(statement));
        return false;
    }

    const QVariant rawValue = query.value(0);
    bool converted = false;
    const int integerValue = rawValue.toInt(&converted);
    query.finish();

    if (!converted)
    {
        setErrorMessage(
            errorMessage,
            QStringLiteral("PRAGMA Returned a Non-Integer Value: %1")
                .arg(statement));
        return false;
    }

    if (value != nullptr)
    {
        *value = integerValue;
    }
    return true;
}

bool ensureApplicationId(
    QSqlDatabase& database,
    int currentApplicationId,
    QString* errorMessage)
{
    if (currentApplicationId == kGluttonyApplicationId)
    {
        return true;
    }

    if (currentApplicationId != 0)
    {
        setErrorMessage(
            errorMessage,
            QStringLiteral(
                "SQLite Application ID Mismatch. Expected=%1, Actual=%2")
                .arg(kGluttonyApplicationId)
                .arg(currentApplicationId));
        return false;
    }

    SqliteImmediateTransaction transaction(database);
    QString transactionError;
    if (!transaction.begin(&transactionError))
    {
        setErrorMessage(
            errorMessage,
            QStringLiteral("Failed to Begin Application ID Transaction: %1")
                .arg(transactionError));
        return false;
    }

    {
        QSqlQuery query(database);
        if (!query.exec(
                QStringLiteral("PRAGMA application_id = %1;")
                    .arg(kGluttonyApplicationId)))
        {
            setErrorMessage(
                errorMessage,
                QStringLiteral("Failed to Set SQLite Application ID: %1")
                    .arg(SqliteRepositoryUtils::formatSqlError(
                        query.lastError())));
            return false;
        }
        query.finish();
    }

    int verifiedApplicationId = 0;
    if (!readIntegerPragma(
            database,
            QStringLiteral("PRAGMA application_id;"),
            &verifiedApplicationId,
            errorMessage))
    {
        return false;
    }

    if (verifiedApplicationId != kGluttonyApplicationId)
    {
        setErrorMessage(
            errorMessage,
            QStringLiteral(
                "SQLite Application ID Verification Failed. "
                "Expected=%1, Actual=%2")
                .arg(kGluttonyApplicationId)
                .arg(verifiedApplicationId));
        return false;
    }

    if (!transaction.commit(&transactionError))
    {
        setErrorMessage(
            errorMessage,
            QStringLiteral("Failed to Commit SQLite Application ID: %1")
                .arg(transactionError));
        return false;
    }

    return true;
}

bool verifyForeignKeys(
    const QSqlDatabase& database,
    QString* errorMessage)
{
    QSqlQuery query(database);
    if (!query.exec(QStringLiteral("PRAGMA foreign_key_check;")))
    {
        setErrorMessage(
            errorMessage,
            QStringLiteral("Failed to Execute Foreign Key Check: %1")
                .arg(SqliteRepositoryUtils::formatSqlError(
                    query.lastError())));
        return false;
    }

    if (query.next())
    {
        const QString tableName = query.value(0).toString();
        const QString rowId = query.value(1).toString();
        const QString parentTable = query.value(2).toString();
        const QString foreignKeyIndex = query.value(3).toString();
        query.finish();

        setErrorMessage(
            errorMessage,
            QStringLiteral(
                "Foreign Key Check Failed. "
                "Table=%1, RowId=%2, Parent=%3, ForeignKeyIndex=%4")
                .arg(tableName)
                .arg(rowId)
                .arg(parentTable)
                .arg(foreignKeyIndex));
        return false;
    }

    query.finish();
    return true;
}

}

std::optional<DatabaseBootstrapReport> DatabaseBootstrapper::initialize(
    const QString& databaseFilePath,
    const QString& connectionName,
    const QVector<MigrationSource>& migrations,
    QString* errorMessage)
{
    if (errorMessage != nullptr)
    {
        errorMessage->clear();
    }

    QString connectionError;
    auto ownedConnection = DataBaseConnection::openSqlite(
        databaseFilePath,
        connectionName,
        &connectionError);

    if (!ownedConnection.has_value())
    {
        DatabaseBootstrapperUtils::setErrorMessage(
            errorMessage,
            QStringLiteral("SQLite Connection Initialization Failed: %1")
                .arg(connectionError));
        return std::nullopt;
    }

    DatabaseBootstrapReport report;
    report.databaseFilePath = databaseFilePath;
    report.sqliteVersion = ownedConnection->sqliteVersion();

    {
        QSqlDatabase database = QSqlDatabase::database(connectionName, false);

        int initialApplicationId = 0;
        QString identityError;
        if (!DatabaseBootstrapperUtils::readIntegerPragma(
                database,
                QStringLiteral("PRAGMA application_id;"),
                &initialApplicationId,
                &identityError))
        {
            DatabaseBootstrapperUtils::setErrorMessage(
                errorMessage,
                identityError);
            return std::nullopt;
        }

        if (initialApplicationId != 0
            && initialApplicationId
                != DatabaseBootstrapperUtils::kGluttonyApplicationId)
        {
            DatabaseBootstrapperUtils::setErrorMessage(
                errorMessage,
                QStringLiteral(
                    "SQLite Application ID Mismatch. Expected=%1, Actual=%2")
                    .arg(DatabaseBootstrapperUtils::kGluttonyApplicationId)
                    .arg(initialApplicationId));
            return std::nullopt;
        }

        QString planError;
        const auto initialPlan =
            MigrationPlanner::build(database, migrations, &planError);

        if (!initialPlan.has_value())
        {
            DatabaseBootstrapperUtils::setErrorMessage(
                errorMessage,
                QStringLiteral("Migration Plan Generation Failed: %1")
                    .arg(planError));
            return std::nullopt;
        }

        if (initialPlan->entries.size() != migrations.size())
        {
            DatabaseBootstrapperUtils::setErrorMessage(
                errorMessage,
                QStringLiteral(
                    "Migration Plan Entry Count Mismatch. Plan=%1, Catalog=%2")
                    .arg(initialPlan->entries.size())
                    .arg(migrations.size()));
            return std::nullopt;
        }

        report.initialUserVersion = initialPlan->databaseUserVersion;
        report.previouslyAppliedCount = initialPlan->appliedCount;

        for (qsizetype index = 0; index < initialPlan->entries.size(); ++index)
        {
            const MigrationPlanEntry& planEntry =
                initialPlan->entries.at(index);
            const MigrationSource& migration = migrations.at(index);

            if (planEntry.version != migration.version
                || planEntry.name != migration.name
                || planEntry.checksumSha256 != migration.checksumSha256)
            {
                DatabaseBootstrapperUtils::setErrorMessage(
                    errorMessage,
                    QStringLiteral(
                        "Migration Plan and Catalog Mismatch at Index %1.")
                        .arg(index));
                return std::nullopt;
            }

            if (planEntry.state == MigrationPlanEntryState::Applied)
            {
                continue;
            }

            QString executionError;
            auto executionResult = MigrationExecutor::applyOne(
                database,
                migration,
                &executionError);

            if (!executionResult.has_value())
            {
                DatabaseBootstrapperUtils::setErrorMessage(
                    errorMessage,
                    QStringLiteral(
                        "Migration Execution Failed: Version=%1, Name=%2, Error=%3")
                        .arg(migration.version)
                        .arg(migration.name)
                        .arg(executionError));
                return std::nullopt;
            }

            report.appliedMigrations.push_back(
                std::move(executionResult.value()));
        }

        QString verificationError;
        const auto verifiedPlan =
            MigrationPlanner::build(database, migrations, &verificationError);

        if (!verifiedPlan.has_value())
        {
            DatabaseBootstrapperUtils::setErrorMessage(
                errorMessage,
                QStringLiteral("Post-Migration Verification Failed: %1")
                    .arg(verificationError));
            return std::nullopt;
        }

        const int expectedFinalVersion = migrations.constLast().version;
        if (!verifiedPlan->historyTableExist
            || verifiedPlan->pendingCount != 0
            || verifiedPlan->appliedCount != migrations.size()
            || verifiedPlan->databaseUserVersion != expectedFinalVersion)
        {
            DatabaseBootstrapperUtils::setErrorMessage(
                errorMessage,
                QStringLiteral(
                    "Post-Migration State is Inconsistent. "
                    "HistoryTableExists=%1, UserVersion=%2, "
                    "Applied=%3, Pending=%4, ExpectedFinalVersion=%5")
                    .arg(verifiedPlan->historyTableExist
                             ? QStringLiteral("true")
                             : QStringLiteral("false"))
                    .arg(verifiedPlan->databaseUserVersion)
                    .arg(verifiedPlan->appliedCount)
                    .arg(verifiedPlan->pendingCount)
                    .arg(expectedFinalVersion));
            return std::nullopt;
        }

        report.finalUserVersion = verifiedPlan->databaseUserVersion;
        report.appliedThisStartupCount = report.appliedMigrations.size();
        report.totalAppliedCount = verifiedPlan->appliedCount;

        identityError.clear();
        if (!DatabaseBootstrapperUtils::ensureApplicationId(
                database,
                initialApplicationId,
                &identityError))
        {
            DatabaseBootstrapperUtils::setErrorMessage(
                errorMessage,
                identityError);
            return std::nullopt;
        }

        report.applicationId =
            DatabaseBootstrapperUtils::kGluttonyApplicationId;

        QString foreignKeyError;
        if (!DatabaseBootstrapperUtils::verifyForeignKeys(
                database,
                &foreignKeyError))
        {
            DatabaseBootstrapperUtils::setErrorMessage(
                errorMessage,
                foreignKeyError);
            return std::nullopt;
        }

        QString recoveryError;
        const auto recoveryReport =
            InspectionRunRepository::recoverInterruptedRuns(
                database,
                QDateTime::currentMSecsSinceEpoch(),
                &recoveryError);

        if (!recoveryReport.has_value())
        {
            DatabaseBootstrapperUtils::setErrorMessage(
                errorMessage,
                QStringLiteral("Startup Recovery Failed: %1")
                    .arg(recoveryError));
            return std::nullopt;
        }

        report.recoveredProcessingItemCount =
            recoveryReport->resetItemCount;
        report.interruptedRunCount =
            recoveryReport->interruptedRunCount;
    }

    ownedConnection.reset();

    if (QSqlDatabase::contains(connectionName))
    {
        DatabaseBootstrapperUtils::setErrorMessage(
            errorMessage,
            QStringLiteral("SQLite Connection Was not Removed: %1")
                .arg(connectionName));
        return std::nullopt;
    }

    return report;
}
