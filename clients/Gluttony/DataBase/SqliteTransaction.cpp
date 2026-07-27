#include "SqliteTransaction.h"

#include <QSqlQuery>

namespace
{

void setErrorMessage(QString* errorMessage, const QString& message)
{
    if (errorMessage != nullptr)
    {
        *errorMessage = message;
    }
}

bool executeControlStatement(
    QSqlDatabase& database,
    const QString& statement,
    QString* errorMessage)
{
    QSqlQuery query(database);
    if (!query.exec(statement))
    {
        setErrorMessage(
            errorMessage,
            QStringLiteral("Failed to Execute '%1': %2")
                .arg(statement)
                .arg(SqliteRepositoryUtils::formatSqlError(
                    query.lastError())));
        return false;
    }

    query.finish();
    return true;
}

}

QString SqliteRepositoryUtils::formatSqlError(const QSqlError& error)
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

bool SqliteRepositoryUtils::validateOpenSqliteDatabase(
    const QSqlDatabase& database,
    QString* errorMessage)
{
    if (errorMessage != nullptr)
    {
        errorMessage->clear();
    }

    if (!database.isValid())
    {
        setErrorMessage(
            errorMessage,
            QStringLiteral("The Database Connection is Invalid."));
        return false;
    }

    if (!database.isOpen())
    {
        setErrorMessage(
            errorMessage,
            QStringLiteral("The Database Connection is Closed."));
        return false;
    }

    if (database.driverName() != QStringLiteral("QSQLITE"))
    {
        setErrorMessage(
            errorMessage,
            QStringLiteral("QSQLITE is Required, Actual Driver=%1")
                .arg(database.driverName()));
        return false;
    }

    return true;
}

SqliteImmediateTransaction::SqliteImmediateTransaction(QSqlDatabase& database)
    : database_(&database)
{
}

SqliteImmediateTransaction::~SqliteImmediateTransaction()
{
    if (active_ && database_ != nullptr)
    {
        QString ignoredError;
        (void)rollback(&ignoredError);
    }
}

bool SqliteImmediateTransaction::begin(QString* errorMessage)
{
    if (active_)
    {
        setErrorMessage(
            errorMessage,
            QStringLiteral("The Transaction is Already Active."));
        return false;
    }

    if (database_ == nullptr
        || !SqliteRepositoryUtils::validateOpenSqliteDatabase(
            *database_,
            errorMessage))
    {
        return false;
    }

    if (!executeControlStatement(
            *database_,
            QStringLiteral("BEGIN IMMEDIATE;"),
            errorMessage))
    {
        return false;
    }

    active_ = true;
    return true;
}

bool SqliteImmediateTransaction::commit(QString* errorMessage)
{
    if (!active_ || database_ == nullptr)
    {
        setErrorMessage(
            errorMessage,
            QStringLiteral("Cannot Commit an Inactive Transaction."));
        return false;
    }

    if (!executeControlStatement(
            *database_,
            QStringLiteral("COMMIT;"),
            errorMessage))
    {
        return false;
    }

    active_ = false;
    return true;
}

bool SqliteImmediateTransaction::rollback(QString* errorMessage)
{
    if (!active_)
    {
        return true;
    }

    if (database_ == nullptr)
    {
        setErrorMessage(
            errorMessage,
            QStringLiteral("The Transaction Database is Unavailable."));
        return false;
    }

    if (!executeControlStatement(
            *database_,
            QStringLiteral("ROLLBACK;"),
            errorMessage))
    {
        return false;
    }

    active_ = false;
    return true;
}

bool SqliteImmediateTransaction::isActive() const noexcept
{
    return active_;
}
