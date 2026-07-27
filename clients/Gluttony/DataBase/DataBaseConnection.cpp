#include "DataBaseConnection.h"

namespace DataBaseConnectionUtils
{

void setErrorMessage(QString* errorMessage, const QString& message)
{
    if(errorMessage != nullptr)
    {
        *errorMessage = message;
    }
}

QString formatSqlError(const QSqlError& error)
{
    QString message = error.text().trimmed();
    if(message.isEmpty())
    {
        message = QStringLiteral("Unknown SQL Error");
    }

    if(!error.nativeErrorCode().isEmpty())
    {
        message += QStringLiteral(" [Native Error Code: %1]").arg(error.nativeErrorCode());

    }
    return message;
}

void closeAndRemoveNamedConnection(const QString& connectionName)
{
    if(connectionName.isEmpty())
    {
        return ;
    }

    if(!QSqlDatabase::contains(connectionName))
    {
        return ;
    }

    {
        QSqlDatabase dataBase = QSqlDatabase::database(connectionName,false);
        if(dataBase.isValid())
        {
            dataBase.close();
        }
    }

    QSqlDatabase::removeDatabase(connectionName);
}

bool executePragma(QSqlDatabase& dataBase, const QString& statement, QString* errorMessage)
{
    QSqlQuery query(dataBase);
    if(!query.exec(statement))
    {
        setErrorMessage(errorMessage,QStringLiteral("Failed to Execute PRAGMA '%1': %2").arg(statement).arg(formatSqlError(query.lastError())));
        return false;
    }
    return true;
}

bool readPragmaValue(QSqlDatabase& dataBase, const QString& statement, QVariant* value, QString* errorMessage)
{
    QSqlQuery query(dataBase);
    if(!query.exec(statement))
    {
        setErrorMessage(errorMessage,QStringLiteral("Failed to Read PRAGMA '%1': %2").arg(statement).arg(formatSqlError(query.lastError())));
        return false;
    }

    if(!query.next())
    {
        setErrorMessage(errorMessage,QStringLiteral("SQLite PRAGMA Returned no Rows: %1").arg(statement));
        return false;
    }

    if(value != nullptr)
    {
        *value = query.value(0);
    }
    return true;
}

bool configureSqliteConnection(QSqlDatabase& dataBase, QString* errorMessage)
{
    constexpr int expectedForeignKeys = 1;
    constexpr int expectedBusyTimeoutMs = 5000;
    constexpr int expectedSynchronousLevel = 1;

    const QString expectedJournalMode = QStringLiteral("wal");

    if(!executePragma(dataBase,QStringLiteral("PRAGMA foreign_keys = ON;"),errorMessage))
    {
        return false;
    }

    if(!executePragma(dataBase,QStringLiteral("PRAGMA busy_timeout = 5000;"),errorMessage))
    {
        return false;
    }

    if(!executePragma(dataBase,QStringLiteral("PRAGMA journal_mode = WAL;"),errorMessage))
    {
        return false;
    }

    if(!executePragma(dataBase,QStringLiteral("PRAGMA synchronous = NORMAL;"),errorMessage))
    {
        return false;
    }

    QVariant foreignKeysValue;
    if(!readPragmaValue(dataBase,QStringLiteral("PRAGMA foreign_keys;"), &foreignKeysValue, errorMessage))
    {
        return false;
    }

    bool foreignKeysConversionOK = false;
    const int actualForeignKeys = foreignKeysValue.toInt(&foreignKeysConversionOK);

    if(!foreignKeysConversionOK || actualForeignKeys != expectedForeignKeys)
    {
        setErrorMessage(errorMessage,QStringLiteral("Failed to Enable SQLite Foreign Keys; Expected 1, Actual %1").arg(foreignKeysValue.toString()));
        return false;
    }

    QVariant busyTimeoutValue;
    if(!readPragmaValue(dataBase, QStringLiteral("PRAGMA busy_timeout;"),&busyTimeoutValue,errorMessage))
    {
        return false;
    }

    bool busyTimeoutConversionOK = false;
    const int actualBusyTimeoutMs = busyTimeoutValue.toInt(&busyTimeoutConversionOK);

    if(!busyTimeoutConversionOK || actualBusyTimeoutMs != expectedBusyTimeoutMs)
    {
        setErrorMessage(errorMessage,QStringLiteral("Failed to Configure SQLite Busy Timeout; Expected %1 ms, Actual %2").arg(expectedBusyTimeoutMs).arg(busyTimeoutValue.toString()));
        return false;
    }

    QVariant journalModeValue;
    if(!readPragmaValue(dataBase,QStringLiteral("PRAGMA journal_mode;"),&journalModeValue,errorMessage))
    {
        return false;
    }
    const QString actualJournalMode = journalModeValue.toString().trimmed().toLower();

    if(actualJournalMode != expectedJournalMode)
    {
        setErrorMessage(errorMessage,QStringLiteral("Failed to Enable SQLite WAL Mode; Expected '%1', Actual '%2'").arg(expectedJournalMode).arg(actualJournalMode));
        return false;
    }

    QVariant synchronousValue;

    if(!readPragmaValue(dataBase,QStringLiteral("PRAGMA synchronous;"),&synchronousValue,errorMessage))
    {
        return false;
    }

    bool synchronousConversionOK = false;

    const int actualSynchronousLevel = synchronousValue.toInt(&synchronousConversionOK);

    if(!synchronousConversionOK || actualSynchronousLevel != expectedSynchronousLevel)
    {
        setErrorMessage(errorMessage,QStringLiteral("Failed to Configure SQLite Synchronous Mode; Expected NORMAL (1), Actual %1").arg(synchronousValue.toString()));
        return false;
    }
    return true;
}

}


DataBaseConnection::DataBaseConnection(QString connectionName, QVersionNumber sqliteVersion)
    : connectionName_(std::move(connectionName))
    , sqliteVersion_(std::move(sqliteVersion))
{

}


DataBaseConnection::~DataBaseConnection()
{
    closeAndRemove();
}


DataBaseConnection::DataBaseConnection(DataBaseConnection&& other) noexcept
    : connectionName_(std::exchange(other.connectionName_,QString()))
    , sqliteVersion_(std::move(other.sqliteVersion_))
{

}


DataBaseConnection& DataBaseConnection::operator=(DataBaseConnection&& other) noexcept
{
    if(this == &other)
    {
        return *this;
    }

    closeAndRemove();

    connectionName_ = std::exchange(other.connectionName_,QString());
    sqliteVersion_ = std::move(other.sqliteVersion_);

    return *this;
}


std::optional<DataBaseConnection> DataBaseConnection::openSqlite(const QString& dataBaseFilepath, const QString& connectionName, QString* errorMessage)
{
    if(errorMessage != nullptr)
    {
        errorMessage->clear();
    }

    if(dataBaseFilepath.trimmed().isEmpty())
    {
        DataBaseConnectionUtils::setErrorMessage(errorMessage,QStringLiteral("The DataBase File Path is Empty"));
        return std::nullopt;
    }

    const QFileInfo dataBaseFileInfo(dataBaseFilepath);

    if(!dataBaseFileInfo.isAbsolute())
    {
        DataBaseConnectionUtils::setErrorMessage(errorMessage,QStringLiteral("The DataBase File Path Must be Absolute: %1").arg(dataBaseFilepath));
        return std::nullopt;
    }

    if(dataBaseFileInfo.exists() && !dataBaseFileInfo.isFile())
    {
        DataBaseConnectionUtils::setErrorMessage(errorMessage,QStringLiteral("The DataBase Path Exists But is not a File: %1").arg(QDir::toNativeSeparators(dataBaseFilepath)));
        return std::nullopt;
    }

    const QString parentDirectoryPath = dataBaseFileInfo.absolutePath();
    const QFileInfo parentDirectoryInfo(parentDirectoryPath);

    if(!parentDirectoryInfo.exists() || !parentDirectoryInfo.isDir())
    {
        DataBaseConnectionUtils::setErrorMessage(errorMessage,QStringLiteral("The DataBase Parent Directory Does not Exist: %1").arg(QDir::toNativeSeparators(parentDirectoryPath)));
        return std::nullopt;
    }

    if(connectionName.trimmed().isEmpty())
    {
        DataBaseConnectionUtils::setErrorMessage(errorMessage,QStringLiteral("The DataBase Connection Name is Empty"));
        return std::nullopt;
    }

    if(QSqlDatabase::contains(connectionName))
    {
        DataBaseConnectionUtils::setErrorMessage(errorMessage,QStringLiteral("A DataBase Connection With This Name Already Exists: %1").arg(connectionName));
        return std::nullopt;
    }

    const QString sqliteDriveName = QStringLiteral("QSQLITE");

    if(!QSqlDatabase::isDriverAvailable(sqliteDriveName))
    {
        const QString availabelDrivers = QSqlDatabase::drivers().join(QStringLiteral(", "));
        DataBaseConnectionUtils::setErrorMessage(errorMessage,QStringLiteral("The QSQL Driver is not Available; Avalable Drivers: %1").arg(availabelDrivers.isEmpty() ? QStringLiteral("<none>") : availabelDrivers));
        return std::nullopt;
    }

    const QString cleanDataBaseFilePath = QDir::cleanPath(dataBaseFileInfo.absoluteFilePath());

    bool connectionReady = false;
    QString failureMessage;
    QVersionNumber detectedSqliteVersion;

    {
        QSqlDatabase dataBase = QSqlDatabase::addDatabase(sqliteDriveName,connectionName);
        if(!dataBase.isValid())
        {
            failureMessage = QStringLiteral("Qt Created an Invalid SQLite DataBase Connection");
        }
        else
        {
            dataBase.setDatabaseName(cleanDataBaseFilePath);
            if(!dataBase.open())
            {
                failureMessage = QStringLiteral("Failed to Open SQLite DataBase '%1': %2").arg(QDir::toNativeSeparators(cleanDataBaseFilePath)).arg(DataBaseConnectionUtils::formatSqlError(dataBase.lastError()));
            }
            else
            {
                QString sqliteVersionText;
                bool versionQuerySucceeded = false;

                {
                    QSqlQuery versionQuery(dataBase);

                    if(!versionQuery.exec(QStringLiteral("SELECT sqlite_version();")))
                    {
                        failureMessage = QStringLiteral("Failed to Query the SQLite Runtime; Version: %1").arg(DataBaseConnectionUtils::formatSqlError(versionQuery.lastError()));

                    }
                    else if(!versionQuery.next())
                    {
                        failureMessage = QStringLiteral("The SQLite Version Query; Returned no Rows");

                    }
                    else
                    {
                        sqliteVersionText = versionQuery.value(0).toString();
                        versionQuerySucceeded = true;
                    }
                }
                if(versionQuerySucceeded)
                {
                    detectedSqliteVersion = QVersionNumber::fromString(sqliteVersionText);
                    if(detectedSqliteVersion.isNull())
                    {
                        failureMessage = QStringLiteral("SQLite Returned an Invalid; Version String: %1").arg(sqliteVersionText);
                    }
                    else
                    {
                        const QVersionNumber minimumVersion(3,37,0);
                        if(QVersionNumber::compare(detectedSqliteVersion,minimumVersion) < 0)
                        {
                            failureMessage = QStringLiteral("SQLite %1 is Too OLD; Gluttony Requires SQLite %2 or Newer").arg(detectedSqliteVersion.toString()).arg(minimumVersion.toString());
                        }
                        else if(!DataBaseConnectionUtils::configureSqliteConnection(dataBase,&failureMessage))
                        {
                            connectionReady = false;
                        }
                        else
                        {
                            connectionReady = true;
                        }
                    }
                }
            }
        }
    }

    if(!connectionReady)
    {
        DataBaseConnectionUtils::closeAndRemoveNamedConnection(connectionName);
        DataBaseConnectionUtils::setErrorMessage(errorMessage, failureMessage.isEmpty() ? QStringLiteral("Unknown SQLite connection error.") : failureMessage);
        return std::nullopt;
    }

    return DataBaseConnection(connectionName,detectedSqliteVersion);
}


const QString& DataBaseConnection::connectionName() const noexcept
{
    return connectionName_;
}


const QVersionNumber& DataBaseConnection::sqliteVersion() const noexcept
{
    return sqliteVersion_;
}


bool DataBaseConnection::isOpen() const
{
    if(connectionName_.isEmpty())
    {
        return false;
    }

    if(!QSqlDatabase::contains(connectionName_))
    {
        return false;
    }

    const QSqlDatabase dataBase = QSqlDatabase::database(connectionName_,false);

    return dataBase.isValid() && dataBase.isOpen();
}


void DataBaseConnection::closeAndRemove() noexcept
{
    if(connectionName_.isEmpty())
    {
        return ;
    }

    DataBaseConnectionUtils::closeAndRemoveNamedConnection(connectionName_);

    connectionName_.clear();
    sqliteVersion_ = QVersionNumber();
}
