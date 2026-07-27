#include "DataBasePaths.h"

namespace DataBasePathsUtils
{
    void setErrorMessage(QString* errorMessage, const QString& message)
    {
        if(errorMessage != nullptr)
        {
            *errorMessage = message;
        }
    }
}

DataBasePaths::DataBasePaths(
    QString applicationDataRoot,
    QString persistenceRoot,
    QString databaseFilePath,
    QString managedRunsRoot)
    : applicationDataRoot_(applicationDataRoot)
    , persistenceRoot_(persistenceRoot)
    , dataBaseFilePath_(databaseFilePath)
    , managedRunsRoot_(managedRunsRoot)
{

}



std::optional<DataBasePaths> DataBasePaths::createDefault(QString *errorMessage)
{
    if(errorMessage != nullptr)
    {
        errorMessage->clear();
    }

    const QString rootPath = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);

    if(rootPath.isEmpty())
    {
        DataBasePathsUtils::setErrorMessage(errorMessage, QStringLiteral("Qt Could not Determine the Application Data Directory."));
        return std::nullopt;
    }
    return createUnderRoot(rootPath, errorMessage);
}


std::optional<DataBasePaths> DataBasePaths::createUnderRoot(const QString& rootPath, QString *errorMessage)
{
    if(errorMessage)
    {
        errorMessage->clear();
    }

    if(rootPath.trimmed().isEmpty())
    {
        DataBasePathsUtils::setErrorMessage(errorMessage,QStringLiteral("The Application Data Root Path is Empty."));
        return std::nullopt;
    }

    const QFileInfo rootInfo(rootPath);

    if(!rootInfo.isAbsolute())
    {
        DataBasePathsUtils::setErrorMessage(errorMessage,QStringLiteral("The Application Data Root Must be an Absolute Path: %1").arg(rootPath));
        return std::nullopt;
    }

    const QString cleanRootPath = QDir::cleanPath(rootInfo.absoluteFilePath());

    const QString persistenceRoot = QDir(cleanRootPath).filePath(QStringLiteral("data/persistence"));

    const QString dataBaseFilePath = QDir(persistenceRoot).filePath(QStringLiteral("gluttony.db"));

    const QString managedRunsRoot = QDir(persistenceRoot).filePath(QStringLiteral("runs"));

    if(!QDir().mkpath(persistenceRoot))
    {
        DataBasePathsUtils::setErrorMessage(errorMessage,QStringLiteral("Failed to Create The Persistence Directory: %1").arg(QDir::toNativeSeparators(persistenceRoot)));
        return std::nullopt;
    }

    const QFileInfo persistenceInfo(persistenceRoot);

    if(!persistenceInfo.exists() || !persistenceInfo.isDir())
    {
        DataBasePathsUtils::setErrorMessage(errorMessage, QStringLiteral("The Persistence Path is not a Directory: %1").arg(QDir::toNativeSeparators(persistenceRoot)));
        return std::nullopt;
    }

    if(!QDir().mkpath(managedRunsRoot))
    {
        DataBasePathsUtils::setErrorMessage(errorMessage,QStringLiteral("Failed to Create the Managed Runs Directory: %1").arg(QDir::toNativeSeparators(managedRunsRoot)));

        return std::nullopt;
    }

    const QFileInfo runsInfo(managedRunsRoot);

    if(!runsInfo.exists() || !runsInfo.isDir())
    {
        DataBasePathsUtils::setErrorMessage(errorMessage,QStringLiteral("The Managed Runs Path is not a Directory: %1").arg(QDir::toNativeSeparators(managedRunsRoot)));
        return std::nullopt;
    }

    const QFileInfo dataBaseInfo(dataBaseFilePath);

    if(dataBaseInfo.exists() && !dataBaseInfo.isFile())
    {
        DataBasePathsUtils::setErrorMessage(errorMessage,QStringLiteral("The Database Path exists but is not a File: %1").arg(QDir::toNativeSeparators(dataBaseFilePath)));
        return std::nullopt;
    }
    return DataBasePaths(
        cleanRootPath,
        QDir::cleanPath(persistenceRoot),
        QDir::cleanPath(dataBaseFilePath),
        QDir::cleanPath(managedRunsRoot));
}


const QString& DataBasePaths::applicationDataRoot() const noexcept
{
    return applicationDataRoot_;
}


const QString& DataBasePaths::persistenceRoot() const noexcept
{
    return persistenceRoot_;
}


const QString& DataBasePaths::dataBaseFilePath() const noexcept
{
    return dataBaseFilePath_;
}


const QString& DataBasePaths::managedRunsRoot() const noexcept
{
    return managedRunsRoot_;
}
