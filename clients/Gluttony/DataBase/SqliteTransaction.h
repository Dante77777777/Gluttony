#ifndef SQLITETRANSACTION_H
#define SQLITETRANSACTION_H

#include <QSqlDatabase>
#include <QSqlError>
#include <QString>

namespace SqliteRepositoryUtils
{

QString formatSqlError(const QSqlError& error);

bool validateOpenSqliteDatabase(
    const QSqlDatabase& database,
    QString* errorMessage = nullptr);

}

class SqliteImmediateTransaction final
{
public:
    explicit SqliteImmediateTransaction(QSqlDatabase& database);
    ~SqliteImmediateTransaction();

    SqliteImmediateTransaction(const SqliteImmediateTransaction&) = delete;
    SqliteImmediateTransaction& operator=(const SqliteImmediateTransaction&) = delete;

    [[nodiscard]]
    bool begin(QString* errorMessage = nullptr);

    [[nodiscard]]
    bool commit(QString* errorMessage = nullptr);

    [[nodiscard]]
    bool rollback(QString* errorMessage = nullptr);

    [[nodiscard]]
    bool isActive() const noexcept;

private:
    QSqlDatabase* database_ = nullptr;
    bool active_ = false;
};

#endif
