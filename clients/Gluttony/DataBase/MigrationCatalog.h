#ifndef MIGRATIONCATALOG_H
#define MIGRATIONCATALOG_H

#include <QByteArray>
#include <QString>
#include <QVector>
#include <QFile>
#include <QFileDevice>
#include <utility>

#include <optional>

#include "MigrationStatementSplitter.h"
#include "MigrationCheckSum.h"

struct MigrationSource final
{
    int version = 0;
    QString name;
    QString resourcePath;
    QByteArray sqlBytes;

    QString checksumSha256;
    QVector<QByteArray> statements;
};

class MigrationCatalog final
{
public:
    [[nodiscard]]
    static std::optional<QVector<MigrationSource>> loadEmbeddedMigrations(QString* errorMessage = nullptr);

};






#endif
