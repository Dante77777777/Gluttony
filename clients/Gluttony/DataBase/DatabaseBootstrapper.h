#ifndef DATABASEBOOTSTRAPPER_H
#define DATABASEBOOTSTRAPPER_H

#include "MigrationCatalog.h"
#include "MigrationExecutor.h"

#include <QString>
#include <QVector>
#include <QVersionNumber>

#include <optional>

struct DatabaseBootstrapReport final
{
    QString databaseFilePath;
    QVersionNumber sqliteVersion;
    int applicationId = 0;
    int initialUserVersion = 0;
    int finalUserVersion = 0;
    qsizetype previouslyAppliedCount = 0;
    qsizetype appliedThisStartupCount = 0;
    qsizetype totalAppliedCount = 0;
    qint64 recoveredProcessingItemCount = 0;
    qint64 interruptedRunCount = 0;
    QVector<MigrationExecutionResult> appliedMigrations;
};

class DatabaseBootstrapper final
{
public:
    [[nodiscard]]
    static std::optional<DatabaseBootstrapReport> initialize(
        const QString& databaseFilePath,
        const QString& connectionName,
        const QVector<MigrationSource>& migrations,
        QString* errorMessage = nullptr);
};

#endif
