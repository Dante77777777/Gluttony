#ifndef MIGRATIONEXECUTOR_H
#define MIGRATIONEXECUTOR_H

#include "MigrationCatalog.h"

#include <QSqlDatabase>
#include <QString>

#include <optional>

struct MigrationExecutionResult final
{
    int version = 0;
    QString name;
    qsizetype executedStatementCount = 0;
    qint64 appliedAtMs = 0;
};

class MigrationExecutor final
{
public:
    [[nodiscard]]
    static std::optional<MigrationExecutionResult> applyOne(
        QSqlDatabase& database,
        const MigrationSource& migration,
        QString* errorMessage = nullptr);
};

#endif
