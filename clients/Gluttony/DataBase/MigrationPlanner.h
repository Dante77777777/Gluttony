#ifndef MIGRATIONPLANNER_h
#define MIGRATIONPLANNER_h

#include "MigrationCatalog.h"
#include "MigrationCheckSum.h"

#include <QString>
#include <QVector>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSet>
#include <QSqlError>
#include <QVariant>

#include <optional>

enum class MigrationPlanEntryState
{
    Applied,
    Pending
};

struct MigrationPlanEntry final
{
    int version = 0;
    QString name;
    QString checksumSha256;

    MigrationPlanEntryState state = MigrationPlanEntryState::Pending;
    qint64 appliedAtMs = 0;
};

struct MigrationPlan final
{
    bool historyTableExist = false;
    int databaseUserVersion = 0;

    qsizetype appliedCount = 0;
    qsizetype pendingCount = 0;

    QVector<MigrationPlanEntry> entries;
};

class MigrationPlanner final
{
public:
    [[nodiscard]]
    static std::optional<MigrationPlan> build(const QSqlDatabase& database, const QVector<MigrationSource>& migrations, QString* errorMessage = nullptr);
};

#endif
