#ifndef MIGRATIONSTATEMENTSPLITTER_H
#define MIGRATIONSTATEMENTSPLITTER_H

#include <QByteArray>
#include <QString>
#include <QVector>
#include <QList>

#include <utility>
#include <optional>


class MigrationStatementSplitter
{
public:
    [[nodiscard]]
    static const QByteArray& statementBreakMarker();

    [[nodiscard]]
    static std::optional<QVector<QByteArray>> split(const QByteArray& sqlBytes, QString* errorMessage = nullptr);

};



#endif
