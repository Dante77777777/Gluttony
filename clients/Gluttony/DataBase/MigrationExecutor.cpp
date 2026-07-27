#include "MigrationExecutor.h"

#include "MigrationCheckSum.h"
#include "SqliteTransaction.h"

#include <QByteArrayView>
#include <QDateTime>
#include <QSqlDriver>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>
#include <QVector>

#include <optional>

namespace MigrationExecutorUtils
{

void setErrorMessage(QString* errorMessage, const QString& message)
{
    if (errorMessage != nullptr)
    {
        *errorMessage = message;
    }
}

QString formatSqlError(const QSqlError& error)
{
    QString message = error.text().trimmed();
    if (message.isEmpty())
    {
        message = QStringLiteral("Unknown SQL Error");
    }

    if (!error.nativeErrorCode().isEmpty())
    {
        message += QStringLiteral(" [Native Error Code: %1]")
                       .arg(error.nativeErrorCode());
    }

    return message;
}

void rollbackAfterFailure(
    SqliteImmediateTransaction& transaction,
    const QString& failureMessage,
    QString* errorMessage)
{
    QString finalMessage = failureMessage;
    QString rollbackError;

    if (transaction.rollback(&rollbackError))
    {
        finalMessage += QStringLiteral(" Transaction was Rolled Back.");
    }
    else
    {
        finalMessage += QStringLiteral(" Transaction Rollback also Failed: %1")
                            .arg(rollbackError);
    }

    setErrorMessage(errorMessage, finalMessage);
}

std::optional<QVector<QString>> validateAndDecodeStatements(
    const MigrationSource& migration,
    QString* errorMessage)
{
    if (migration.version <= 0)
    {
        setErrorMessage(
            errorMessage,
            QStringLiteral("Migration Version Must be Positive: %1")
                .arg(migration.version));
        return std::nullopt;
    }

    if (migration.name.trimmed().isEmpty())
    {
        setErrorMessage(
            errorMessage,
            QStringLiteral("Migration Name is Empty for Version %1")
                .arg(migration.version));
        return std::nullopt;
    }

    if (!MigrationCheckSum::isValidSha256Hex(migration.checksumSha256))
    {
        setErrorMessage(
            errorMessage,
            QStringLiteral("Migration Checksum is Invalid for Version %1")
                .arg(migration.version));
        return std::nullopt;
    }

    if (migration.statements.isEmpty())
    {
        setErrorMessage(
            errorMessage,
            QStringLiteral("Migration Contains no Statements for Version %1")
                .arg(migration.version));
        return std::nullopt;
    }

    QVector<QString> decodedStatements;
    decodedStatements.reserve(migration.statements.size());

    for (qsizetype index = 0; index < migration.statements.size(); ++index)
    {
        const QByteArray& statementBytes = migration.statements.at(index);

        if (statementBytes.contains('\0'))
        {
            setErrorMessage(
                errorMessage,
                QStringLiteral(
                    "Migration Version %1 Statement %2 Contains a NUL Byte.")
                    .arg(migration.version)
                    .arg(index + 1));
            return std::nullopt;
        }

        const QByteArrayView statementView(
            statementBytes.constData(),
            statementBytes.size());

        if (!statementView.isValidUtf8())
        {
            setErrorMessage(
                errorMessage,
                QStringLiteral(
                    "Migration Version %1 Statement %2 is not Valid UTF-8.")
                    .arg(migration.version)
                    .arg(index + 1));
            return std::nullopt;
        }

        const QString sql = QString::fromUtf8(statementBytes);
        const QString trimmedSql = sql.trimmed();

        if (trimmedSql.isEmpty())
        {
            setErrorMessage(
                errorMessage,
                QStringLiteral("Migration Version %1 Statement %2 is Empty.")
                    .arg(migration.version)
                    .arg(index + 1));
            return std::nullopt;
        }

        if (!trimmedSql.endsWith(QLatin1Char(';')))
        {
            setErrorMessage(
                errorMessage,
                QStringLiteral(
                    "Migration Version %1 Statement %2 Does not End With a Semicolon.")
                    .arg(migration.version)
                    .arg(index + 1));
            return std::nullopt;
        }

        decodedStatements.push_back(sql);
    }

    return decodedStatements;
}

bool readUserVersion(
    const QSqlDatabase& database,
    int* userVersion,
    QString* errorMessage)
{
    QSqlQuery query(database);

    if (!query.exec(QStringLiteral("PRAGMA user_version;")))
    {
        setErrorMessage(
            errorMessage,
            QStringLiteral("Failed to Read SQLite user_version: %1")
                .arg(formatSqlError(query.lastError())));
        return false;
    }

    if (!query.next())
    {
        query.finish();
        setErrorMessage(
            errorMessage,
            QStringLiteral("SQLite PRAGMA user_version Returned no Rows."));
        return false;
    }

    const QVariant value = query.value(0);
    bool conversionSucceeded = false;
    const int actualVersion = value.toInt(&conversionSucceeded);
    query.finish();

    if (!conversionSucceeded || actualVersion < 0)
    {
        setErrorMessage(
            errorMessage,
            QStringLiteral("SQLite Returned an Invalid user_version: %1")
                .arg(value.toString()));
        return false;
    }

    if (userVersion != nullptr)
    {
        *userVersion = actualVersion;
    }

    return true;
}

bool insertMigrationHistory(
    const QSqlDatabase& database,
    const MigrationSource& migration,
    qint64 appliedAtMs,
    QString* errorMessage)
{
    QSqlQuery query(database);

    if (!query.prepare(
            QStringLiteral(
                "INSERT INTO schema_migration ("
                "version, name, checksum, applied_at_ms"
                ") VALUES ("
                ":version, :name, :checksum, :applied_at_ms"
                ");")))
    {
        setErrorMessage(
            errorMessage,
            QStringLiteral(
                "Failed to Prepare Migration History Insert for Version %1: %2")
                .arg(migration.version)
                .arg(formatSqlError(query.lastError())));
        return false;
    }

    query.bindValue(QStringLiteral(":version"), migration.version);
    query.bindValue(QStringLiteral(":name"), migration.name);
    query.bindValue(QStringLiteral(":checksum"), migration.checksumSha256);
    query.bindValue(QStringLiteral(":applied_at_ms"), appliedAtMs);

    if (!query.exec())
    {
        setErrorMessage(
            errorMessage,
            QStringLiteral(
                "Failed to Insert Migration History for Version %1: %2")
                .arg(migration.version)
                .arg(formatSqlError(query.lastError())));
        return false;
    }

    const qint64 affectedRows = query.numRowsAffected();
    query.finish();

    if (affectedRows != 1)
    {
        setErrorMessage(
            errorMessage,
            QStringLiteral(
                "Migration History Insert Affected an Unexpected Number of Rows "
                "for Version %1. Expected=1, Actual=%2")
                .arg(migration.version)
                .arg(affectedRows));
        return false;
    }

    return true;
}

bool setUserVersion(
    const QSqlDatabase& database,
    int version,
    QString* errorMessage)
{
    const QString statement =
        QStringLiteral("PRAGMA user_version = %1;").arg(version);

    QSqlQuery query(database);
    if (!query.exec(statement))
    {
        setErrorMessage(
            errorMessage,
            QStringLiteral("Failed to Set SQLite user_version to %1: %2")
                .arg(version)
                .arg(formatSqlError(query.lastError())));
        return false;
    }

    query.finish();
    return true;
}

}

std::optional<MigrationExecutionResult> MigrationExecutor::applyOne(
    QSqlDatabase& database,
    const MigrationSource& migration,
    QString* errorMessage)
{
    if (errorMessage != nullptr)
    {
        errorMessage->clear();
    }

    if (!database.isValid())
    {
        MigrationExecutorUtils::setErrorMessage(
            errorMessage,
            QStringLiteral(
                "Cannot Apply Migration Using an Invalid Database Connection."));
        return std::nullopt;
    }

    if (!database.isOpen())
    {
        MigrationExecutorUtils::setErrorMessage(
            errorMessage,
            QStringLiteral(
                "Cannot Apply Migration Using a Closed Database Connection."));
        return std::nullopt;
    }

    if (database.driverName() != QStringLiteral("QSQLITE"))
    {
        MigrationExecutorUtils::setErrorMessage(
            errorMessage,
            QStringLiteral("Migration Executor Requires QSQLITE, Actual Driver=%1")
                .arg(database.driverName()));
        return std::nullopt;
    }

    if (database.driver() == nullptr
        || !database.driver()->hasFeature(QSqlDriver::Transactions))
    {
        MigrationExecutorUtils::setErrorMessage(
            errorMessage,
            QStringLiteral(
                "The SQLite Driver Does not Report Transaction Support."));
        return std::nullopt;
    }

    const auto decodedStatements =
        MigrationExecutorUtils::validateAndDecodeStatements(
            migration,
            errorMessage);

    if (!decodedStatements.has_value())
    {
        return std::nullopt;
    }

    SqliteImmediateTransaction transaction(database);
    QString transactionError;
    if (!transaction.begin(&transactionError))
    {
        MigrationExecutorUtils::setErrorMessage(
            errorMessage,
            QStringLiteral(
                "Failed to Begin Transaction for Migration Version %1: %2")
                .arg(migration.version)
                .arg(transactionError));
        return std::nullopt;
    }

    int currentUserVersion = 0;
    QString operationError;

    if (!MigrationExecutorUtils::readUserVersion(
            database,
            &currentUserVersion,
            &operationError))
    {
        MigrationExecutorUtils::rollbackAfterFailure(
            transaction,
            QStringLiteral(
                "Migration Version %1 Precondition Check Failed: %2")
                .arg(migration.version)
                .arg(operationError),
            errorMessage);
        return std::nullopt;
    }

    const int expectedCurrentVersion = migration.version - 1;
    if (currentUserVersion != expectedCurrentVersion)
    {
        MigrationExecutorUtils::rollbackAfterFailure(
            transaction,
            QStringLiteral(
                "Migration Version %1 Cannot be Applied. "
                "Expected Current user_version=%2, Actual=%3.")
                .arg(migration.version)
                .arg(expectedCurrentVersion)
                .arg(currentUserVersion),
            errorMessage);
        return std::nullopt;
    }

    for (qsizetype index = 0; index < decodedStatements->size(); ++index)
    {
        const QString& sql = decodedStatements->at(index);
        QString statementFailure;

        {
            QSqlQuery query(database);
            if (!query.exec(sql))
            {
                statementFailure =
                    QStringLiteral(
                        "Migration Version %1 Statement %2/%3 Failed: %4. "
                        "Preview='%5'")
                        .arg(migration.version)
                        .arg(index + 1)
                        .arg(decodedStatements->size())
                        .arg(MigrationExecutorUtils::formatSqlError(
                            query.lastError()))
                        .arg(sql.left(160).simplified());
            }
            query.finish();
        }

        if (!statementFailure.isEmpty())
        {
            MigrationExecutorUtils::rollbackAfterFailure(
                transaction,
                statementFailure,
                errorMessage);
            return std::nullopt;
        }
    }

    const qint64 appliedAtMs = QDateTime::currentMSecsSinceEpoch();
    if (appliedAtMs <= 0)
    {
        MigrationExecutorUtils::rollbackAfterFailure(
            transaction,
            QStringLiteral(
                "The System Clock Returned an Invalid Applied Timestamp "
                "for Migration Version %1.")
                .arg(migration.version),
            errorMessage);
        return std::nullopt;
    }

    operationError.clear();
    if (!MigrationExecutorUtils::insertMigrationHistory(
            database,
            migration,
            appliedAtMs,
            &operationError))
    {
        MigrationExecutorUtils::rollbackAfterFailure(
            transaction,
            operationError,
            errorMessage);
        return std::nullopt;
    }

    operationError.clear();
    if (!MigrationExecutorUtils::setUserVersion(
            database,
            migration.version,
            &operationError))
    {
        MigrationExecutorUtils::rollbackAfterFailure(
            transaction,
            operationError,
            errorMessage);
        return std::nullopt;
    }

    int verifiedUserVersion = 0;
    operationError.clear();
    if (!MigrationExecutorUtils::readUserVersion(
            database,
            &verifiedUserVersion,
            &operationError))
    {
        MigrationExecutorUtils::rollbackAfterFailure(
            transaction,
            operationError,
            errorMessage);
        return std::nullopt;
    }

    if (verifiedUserVersion != migration.version)
    {
        MigrationExecutorUtils::rollbackAfterFailure(
            transaction,
            QStringLiteral(
                "SQLite user_version Verification Failed for Migration "
                "Version %1. Expected=%2, Actual=%3")
                .arg(migration.version)
                .arg(migration.version)
                .arg(verifiedUserVersion),
            errorMessage);
        return std::nullopt;
    }

    transactionError.clear();
    if (!transaction.commit(&transactionError))
    {
        const QString commitFailure =
            QStringLiteral("Failed to Commit Migration Version %1: %2")
                .arg(migration.version)
                .arg(transactionError);

        MigrationExecutorUtils::rollbackAfterFailure(
            transaction,
            commitFailure,
            errorMessage);
        return std::nullopt;
    }

    MigrationExecutionResult result;
    result.version = migration.version;
    result.name = migration.name;
    result.executedStatementCount = decodedStatements->size();
    result.appliedAtMs = appliedAtMs;
    return result;
}
