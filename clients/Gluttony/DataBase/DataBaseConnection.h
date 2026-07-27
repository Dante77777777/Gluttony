#ifndef DATABASECONNECTION_H
#define DATABASECONNECTION_H

#include <QString>
#include <QVersionNumber>
#include <QDir>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>
#include <QVariant>

#include <utility>
#include <optional>

class DataBaseConnection final
{
public:
    [[nodiscard]]
    static std::optional<DataBaseConnection> openSqlite(const QString& dataBaseFilePath, const QString& connectionName, QString* errorMessage = nullptr);

    ~DataBaseConnection();

    DataBaseConnection(const DataBaseConnection&) = delete;

    DataBaseConnection& operator=(const DataBaseConnection&) = delete;

    DataBaseConnection(DataBaseConnection&&) noexcept;

    DataBaseConnection& operator=(DataBaseConnection&&) noexcept;

    [[nodiscard]]
    const QString& connectionName() const noexcept;

    [[nodiscard]]
    const QVersionNumber &sqliteVersion() const noexcept;

    [[nodiscard]]
    bool isOpen() const;
private:
    DataBaseConnection(QString connectionName, QVersionNumber sqliteVersion);
    void closeAndRemove() noexcept;

private:
    QString connectionName_;
    QVersionNumber sqliteVersion_;
};




#endif
