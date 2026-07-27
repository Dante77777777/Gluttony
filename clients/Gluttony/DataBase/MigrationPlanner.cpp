#include "MigrationPlanner.h"

namespace MigrationPlannerUtils
{

struct AppliedMigrationRecord final
{
    int Version = 0;
    QString name;
    QString checksumSha256;
    qint64 appliedAtMs = 0;
};

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

bool validationMigrationCatalog(const QVector<MigrationSource>& migrations, QString* errorMessage)
{
    if(migrations.isEmpty())
    {
        setErrorMessage(errorMessage,QStringLiteral("Embedded Migration Catalog is Empty"));
        return false;
    }

    QSet<QString> migrationNames;

    int expectedVersion = 1;
    for(const MigrationSource& migration : migrations)
    {
        if(migration.version != expectedVersion)
        {
            setErrorMessage(errorMessage,QStringLiteral("Embedded Migration Versions Must be Consecutive Starting at 1; Expected=%1, Actual=%2").arg(expectedVersion).arg(migration.version));
            return false;
        }

        if(migration.name.trimmed().isEmpty())
        {
            setErrorMessage(errorMessage,QStringLiteral("Embedded Migration Name is Empty for Version %1").arg(migration.version));
            return false;
        }

        if(migrationNames.contains(migration.name))
        {
            setErrorMessage(errorMessage,QStringLiteral("Embedded Migration Name is Duplicated: %1").arg(migration.name));
            return false;
        }

        if(!MigrationCheckSum::isValidSha256Hex(migration.checksumSha256))
        {
            setErrorMessage(errorMessage,QStringLiteral("Embedded Migration Checksum is Invalid for Version %1").arg(migration.version));
            return false;
        }

        if(migration.sqlBytes.trimmed().isEmpty())
        {
            setErrorMessage(errorMessage,QStringLiteral("Embedded Migration SQL is Empty for Version %1").arg(migration.version));
            return false;
        }

        if(migration.statements.isEmpty())
        {
            setErrorMessage(errorMessage,QStringLiteral("Embedded Migration Contains no Statements for Version %1").arg(migration.version));
            return false;
        }

        migrationNames.insert(migration.name);
        ++expectedVersion;
    }
    return true;
}

bool readDatabaseUserVersion(const QSqlDatabase& database, int* userVersion, QString* errorMessage)
{
    QSqlQuery query(database);

    if(!query.exec(QStringLiteral("PRAGMA user_version;")))
    {
        setErrorMessage(errorMessage,QStringLiteral("Failed to Read SQLite user_version: %1").arg(formatSqlError(query.lastError())));
        return false;
    }

    if(!query.next())
    {
        setErrorMessage(errorMessage,QStringLiteral("SQLite PRAGMA user_version Returned no Rows"));
        return false;
    }

    const QVariant value = query.value(0);

    bool conversionSucceded = false;

    const int actualUserVersion = value.toInt(&conversionSucceded);

    if(!conversionSucceded || actualUserVersion < 0)
    {
        setErrorMessage(errorMessage,QStringLiteral("SQLite Returned an Invalid user_version: %1").arg(value.toString()));
        return false;
    }

    if(userVersion != nullptr)
    {
        *userVersion = actualUserVersion;
    }
    return true;
}

bool readHistoryTableExists(const QSqlDatabase& database, bool* exists, QString* errorMessage)
{
    QSqlQuery query(database);
    if(!query.prepare(QStringLiteral(
            "SELECT 1 "
            "FROM sqlite_schema "
            "WHERE type = 'table' "
            "AND name = :object_name "
            "LIMIT 1;")))
    {
        setErrorMessage(errorMessage,QStringLiteral("Failed to Prepare Migration History Table Lookup: %1").arg(formatSqlError(query.lastError())));
        return false;
    }

    query.bindValue(QStringLiteral(":object_name"),QStringLiteral("schema_migration"));

    if(!query.exec())
    {
        setErrorMessage(errorMessage,QStringLiteral("Failed to Check Migration History Table: %1").arg(formatSqlError(query.lastError())));
        return false;
    }

    if(exists != nullptr)
    {
        *exists = query.next();
    }

    return true;
}

std::optional<QVector<AppliedMigrationRecord>>
readAppliedMigrations(
    const QSqlDatabase& database,
    QString* errorMessage)
{
    QSqlQuery query(database);

    if (!query.exec(
            QStringLiteral(
                "SELECT "
                "version, "
                "name, "
                "checksum, "
                "applied_at_ms "
                "FROM schema_migration "
                "ORDER BY version ASC;"))) {
        setErrorMessage(
            errorMessage,
            QStringLiteral(
                "Failed to Read Applied "
                "Migrations: %1")
                .arg(
                    formatSqlError(
                        query.lastError())));

        return std::nullopt;
    }

    QVector<AppliedMigrationRecord> records;

    while (query.next()) {
        const QVariant versionValue =
            query.value(0);

        const QVariant nameValue =
            query.value(1);

        const QVariant checksumValue =
            query.value(2);

        const QVariant appliedAtValue =
            query.value(3);

        if (versionValue.isNull()
            || nameValue.isNull()
            || checksumValue.isNull()
            || appliedAtValue.isNull()) {
            setErrorMessage(
                errorMessage,
                QStringLiteral(
                    "Migration History Contains "
                    "Unexpected NULL Values."));

            return std::nullopt;
        }

        bool versionConversionSucceeded = false;

        const int version =
            versionValue.toInt(
                &versionConversionSucceeded);

        if (!versionConversionSucceeded
            || version <= 0) {
            setErrorMessage(
                errorMessage,
                QStringLiteral(
                    "Migration History Contains an "
                    "Invalid Version: %1")
                    .arg(versionValue.toString()));

            return std::nullopt;
        }

        const QString name =
            nameValue.toString();

        if (name.trimmed().isEmpty()) {
            setErrorMessage(
                errorMessage,
                QStringLiteral(
                    "Migration History Contains an "
                    "Empty Name for Version %1")
                    .arg(version));

            return std::nullopt;
        }

        const QString checksum =
            checksumValue.toString();

        if (!MigrationCheckSum::
            isValidSha256Hex(
                checksum)) {
            setErrorMessage(
                errorMessage,
                QStringLiteral(
                    "Migration History Contains an "
                    "Invalid SHA-256 Checksum for "
                    "Version %1: %2")
                    .arg(version)
                    .arg(checksum));

            return std::nullopt;
        }

        bool appliedAtConversionSucceeded =
            false;

        const qint64 appliedAtMs =
            appliedAtValue.toLongLong(
                &appliedAtConversionSucceeded);

        if (!appliedAtConversionSucceeded
            || appliedAtMs <= 0) {
            setErrorMessage(
                errorMessage,
                QStringLiteral(
                    "Migration History Contains an "
                    "Invalid Applied Timestamp for "
                    "Version %1: %2")
                    .arg(version)
                    .arg(
                        appliedAtValue.toString()));

            return std::nullopt;
        }

        AppliedMigrationRecord record;

        record.Version = version;
        record.name = name;
        record.checksumSha256 = checksum;
        record.appliedAtMs = appliedAtMs;

        records.push_back(
            std::move(record));
    }

    if (query.lastError().isValid()) {
        setErrorMessage(
            errorMessage,
            QStringLiteral(
                "Failed While Iterating Migration "
                "History Rows: %1")
                .arg(
                    formatSqlError(
                        query.lastError())));

        return std::nullopt;
    }

    return records;
}

}


std::optional<MigrationPlan>
MigrationPlanner::build(
    const QSqlDatabase& database,
    const QVector<MigrationSource>& migrations,
    QString* errorMessage)
{
    if (errorMessage != nullptr) {
        errorMessage->clear();
    }

    if (!database.isValid()) {
        MigrationPlannerUtils::setErrorMessage(
            errorMessage,
            QStringLiteral(
                "Cannot Build Migration Plan "
                "from an Invalid Database "
                "Connection."));

        return std::nullopt;
    }

    if (!database.isOpen()) {
        MigrationPlannerUtils::setErrorMessage(
            errorMessage,
            QStringLiteral(
                "Cannot Build Migration Plan "
                "from a Closed Database "
                "Connection."));

        return std::nullopt;
    }

    if (database.driverName()
        != QStringLiteral("QSQLITE")) {
        MigrationPlannerUtils::setErrorMessage(
            errorMessage,
            QStringLiteral(
                "Migration Planner Requires "
                "QSQLITE, Actual Driver=%1")
                .arg(
                    database.driverName()));

        return std::nullopt;
    }

    if (!MigrationPlannerUtils::validationMigrationCatalog(
            migrations,
            errorMessage)) {
        return std::nullopt;
    }

    int databaseUserVersion = 0;

    if (!MigrationPlannerUtils::readDatabaseUserVersion(
            database,
            &databaseUserVersion,
            errorMessage)) {
        return std::nullopt;
    }

    bool historyTableExists = false;

    if (!MigrationPlannerUtils::readHistoryTableExists(
            database,
            &historyTableExists,
            errorMessage)) {
        return std::nullopt;
    }

    QVector<MigrationPlannerUtils::AppliedMigrationRecord>
        appliedRecords;

    if (!historyTableExists) {
        if (databaseUserVersion != 0) {
            MigrationPlannerUtils::setErrorMessage(
                errorMessage,
                QStringLiteral(
                    "SQLite user_version is %1, "
                    "but schema_migration Does "
                    "not Exist.")
                    .arg(databaseUserVersion));

            return std::nullopt;
        }
    } else {
        const auto readResult =
            MigrationPlannerUtils::readAppliedMigrations(
                database,
                errorMessage);

        if (!readResult.has_value()) {
            return std::nullopt;
        }

        appliedRecords =
            readResult.value();

        if (appliedRecords.isEmpty()) {
            MigrationPlannerUtils::setErrorMessage(
                errorMessage,
                QStringLiteral(
                    "schema_migration Exists but "
                    "Contains no Applied Migration "
                    "Records."));

            return std::nullopt;
        }
    }

    if (appliedRecords.size()
        > migrations.size()) {
        MigrationPlannerUtils::setErrorMessage(
            errorMessage,
            QStringLiteral(
                "Database Contains %1 Applied "
                "Migrations, but This Application "
                "Only Contains %2 Migrations. "
                "The Database May be Newer than "
                "the Application.")
                .arg(appliedRecords.size())
                .arg(migrations.size()));

        return std::nullopt;
    }

    // 数据库中的迁移必须是内嵌迁移目录的完整前缀。
    for (qsizetype index = 0;
         index < appliedRecords.size();
         ++index) {
        const MigrationPlannerUtils::AppliedMigrationRecord& actual =
            appliedRecords.at(index);

        const MigrationSource& expected =
            migrations.at(index);

        if (actual.Version
            != expected.version) {
            MigrationPlannerUtils::setErrorMessage(
                errorMessage,
                QStringLiteral(
                    "Migration History is not an "
                    "Applied Prefix. At Index %1, "
                    "Expected Version=%2, "
                    "Actual Version=%3")
                    .arg(index)
                    .arg(expected.version)
                    .arg(actual.Version));

            return std::nullopt;
        }

        if (actual.name != expected.name) {
            MigrationPlannerUtils::setErrorMessage(
                errorMessage,
                QStringLiteral(
                    "Migration Name Mismatch for "
                    "Version %1. "
                    "Expected='%2', Actual='%3'")
                    .arg(expected.version)
                    .arg(expected.name)
                    .arg(actual.name));

            return std::nullopt;
        }

        if (actual.checksumSha256
            != expected.checksumSha256) {
            MigrationPlannerUtils::setErrorMessage(
                errorMessage,
                QStringLiteral(
                    "Migration Checksum Mismatch "
                    "for Version %1. "
                    "Expected='%2', Actual='%3'")
                    .arg(expected.version)
                    .arg(expected.checksumSha256)
                    .arg(actual.checksumSha256));

            return std::nullopt;
        }
    }

    const int expectedUserVersion =
        appliedRecords.isEmpty()
            ? 0
            : appliedRecords.constLast().Version;

    if (databaseUserVersion
        != expectedUserVersion) {
        MigrationPlannerUtils::setErrorMessage(
            errorMessage,
            QStringLiteral(
                "SQLite user_version Does not "
                "Match Migration History. "
                "Expected=%1, Actual=%2")
                .arg(expectedUserVersion)
                .arg(databaseUserVersion));

        return std::nullopt;
    }

    MigrationPlan plan;

    plan.historyTableExist =
        historyTableExists;

    plan.databaseUserVersion =
        databaseUserVersion;

    plan.appliedCount =
        appliedRecords.size();

    plan.pendingCount =
        migrations.size()
        - appliedRecords.size();

    plan.entries.reserve(
        migrations.size());

    for (qsizetype index = 0;
         index < migrations.size();
         ++index) {
        const MigrationSource& migration =
            migrations.at(index);

        const bool alreadyApplied =
            index < appliedRecords.size();

        MigrationPlanEntry entry;

        entry.version =
            migration.version;

        entry.name =
            migration.name;

        entry.checksumSha256 =
            migration.checksumSha256;

        entry.state =
            alreadyApplied
                ? MigrationPlanEntryState::Applied
                : MigrationPlanEntryState::Pending;

        if (alreadyApplied) {
            entry.appliedAtMs =
                appliedRecords
                    .at(index)
                    .appliedAtMs;
        }

        plan.entries.push_back(
            std::move(entry));
    }

    return plan;
}
