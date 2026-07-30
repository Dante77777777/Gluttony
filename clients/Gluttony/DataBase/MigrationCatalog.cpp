#include "MigrationCatalog.h"

namespace MigrationCatalogUtils {

void setErrorMessage(QString* errorMessage,const QString& message)
{
    if(errorMessage)
    {
        *errorMessage = message;
    }

}

std::optional<MigrationSource> loadMigration(int version, const QString& name, const QString& resourcePath, qsizetype expectedStatementCount, QString* errorMessage)
{
    if (expectedStatementCount <= 0)
    {
        setErrorMessage(errorMessage,QStringLiteral("Expected Migration Statement ""Count Must be Positive: %1").arg(expectedStatementCount));

        return std::nullopt;
    }

    if(version <= 0)
    {
        setErrorMessage(errorMessage,QStringLiteral("Migration Version Must be Positive: %1").arg(version));
        return std::nullopt;
    }

    if(name.trimmed().isEmpty())
    {
        setErrorMessage(errorMessage,QStringLiteral("Migration Name is Empty for Version %1").arg(version));
        return std::nullopt;
    }

    if(resourcePath.trimmed().isEmpty())
    {
        setErrorMessage(errorMessage,QStringLiteral("Migration Resource Path is Empty for Version %1").arg(version));
        return std::nullopt;
    }

    if(!QFile::exists(resourcePath))
    {
        setErrorMessage(errorMessage,QStringLiteral("Embedded Migration Resource Does not Exist: %1").arg(resourcePath));
        return std::nullopt;
    }

    QFile migrationFile(resourcePath);

    if(!migrationFile.open(QIODeviceBase::ReadOnly))
    {
        setErrorMessage(errorMessage,QStringLiteral("Failed to Open Embedded Migration '%1': %2").arg(resourcePath).arg(migrationFile.errorString()));
        return std::nullopt;
    }

    QByteArray sqlBytes = migrationFile.readAll();

    if(migrationFile.error() != QFileDevice::NoError)
    {
        setErrorMessage(errorMessage,QStringLiteral("Failed to Read Embedded Migration '%1': %2").arg(resourcePath).arg(migrationFile.errorString()));
        return std::nullopt;
    }

    if(sqlBytes.contains('\0'))
    {
        setErrorMessage(errorMessage,QStringLiteral("Embedded Migration Contains Unexpected NUL Bytes: %1").arg(resourcePath));
        return std::nullopt;
    }

    MigrationCheckSumResult checksumResult = MigrationCheckSum::calculate(sqlBytes);

    if(checksumResult.normalizedSqlBytes.trimmed().isEmpty())
    {
        setErrorMessage(errorMessage,QStringLiteral("Embedded Migration is Empty After Normalization: %1").arg(resourcePath));
        return std::nullopt;
    }

    if(!MigrationCheckSum::isValidSha256Hex(checksumResult.sha256Hex))
    {
        setErrorMessage(errorMessage,QStringLiteral("Embedded Migration SHA-256 Checksum is Invalid: %1").arg(resourcePath));
        return std::nullopt;
    }

    QString splitErrorMessage;

    auto statements = MigrationStatementSplitter::split(checksumResult.normalizedSqlBytes,&splitErrorMessage);
    if(!statements.has_value())
    {
        setErrorMessage(errorMessage,QStringLiteral("Failed to Split Embedded Migration '%1': %2").arg(name, splitErrorMessage));
        return std::nullopt;
    }
    if(statements->size() != expectedStatementCount)
    {
        setErrorMessage(errorMessage,QStringLiteral("Embedded Migration '%1' Statement Count Mismatch. Expected=%2, Actual=%3").arg(name).arg(expectedStatementCount).arg(statements->size()));
        return std::nullopt;
    }

    MigrationSource migration;

    migration.version = version;
    migration.name = name;
    migration.resourcePath = resourcePath;
    migration.sqlBytes = std::move(checksumResult.normalizedSqlBytes);
    migration.checksumSha256 = std::move(checksumResult.sha256Hex);
    migration.statements = std::move(statements.value());

    return migration;
}


}

 std::optional<QVector<MigrationSource>> MigrationCatalog::loadEmbeddedMigrations(QString* errorMessage)
{
    if(errorMessage)
    {
        errorMessage->clear();
    }

    QVector<MigrationSource> migrations;

    auto initialSchema = MigrationCatalogUtils::loadMigration(1,QStringLiteral("001_initial_schema"),QStringLiteral(":/database/migrations/001_initial_schema.sql"),18,errorMessage);

    if(!initialSchema.has_value())
    {
        return std::nullopt;
    }
    migrations.push_back(std::move(initialSchema.value()));

    auto tcpCameraPersistence = MigrationCatalogUtils::loadMigration(
        2,
        QStringLiteral("002_tcp_camera_persistence"),
        QStringLiteral(
            ":/database/migrations/002_tcp_camera_persistence.sql"),
        23,
        errorMessage);

    if(!tcpCameraPersistence.has_value())
    {
        return std::nullopt;
    }
    migrations.push_back(std::move(tcpCameraPersistence.value()));

    return migrations;
}
