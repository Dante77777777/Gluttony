#include "MigrationStatementSplitter.h"

namespace MigrationStatementSplitterUtils
{

void setErrorMessage(QString* errorMessage, const QString& message)
{
    if(errorMessage != nullptr)
    {
        *errorMessage = message;
    }
}

}


const QByteArray& MigrationStatementSplitter::statementBreakMarker()
{
    static const QByteArray marker("-- gluttony:statement-break");
    return marker;
}


std::optional<QVector<QByteArray>> MigrationStatementSplitter::split(const QByteArray& sqlBytes, QString* errorMessage)
{
    if(errorMessage != nullptr)
    {
        errorMessage->clear();
    }

    if(sqlBytes.trimmed().isEmpty())
    {
        MigrationStatementSplitterUtils::setErrorMessage(errorMessage,QStringLiteral("Migration SQL is Empty"));
        return std::nullopt;
    }

    QByteArray normalizedSql = sqlBytes;

    normalizedSql.replace("\r\n","\n");

    normalizedSql.replace('\r','\n');

    const QList<QByteArray> lines = normalizedSql.split('\n');

    QVector<QByteArray> statements;
    QByteArray currentStatement;

    qsizetype lineNumber = 0;
    qsizetype markerCount = 0;

    for(const QByteArray& line : lines)
    {
        ++lineNumber;
        const QByteArray trimmedLine = line.trimmed();
        if(trimmedLine == statementBreakMarker())
        {
            ++markerCount;

            QByteArray statement = currentStatement.trimmed();
            if(statement.isEmpty())
            {
                MigrationStatementSplitterUtils::setErrorMessage(errorMessage,QStringLiteral("Statement Break Marker at Line %1 Has no Preceding SQL Statement").arg(lineNumber));
                return std::nullopt;
            }

            if(!statement.endsWith(';'))
            {
                MigrationStatementSplitterUtils::setErrorMessage(errorMessage,QStringLiteral("SQL Statement Before Marker at Line %1 Does not End With ';'").arg(lineNumber));
                return std::nullopt;
            }

            statements.push_back(std::move(statement));
            currentStatement.clear();
            continue;
        }
        currentStatement.append(line);
        currentStatement.append('\n');
    }

    if(markerCount == 0)
    {
        MigrationStatementSplitterUtils::setErrorMessage(errorMessage,QStringLiteral("Migration SQL Contains no Statement Break Markers. Expected Marker: %1").arg(QString::fromLatin1(statementBreakMarker())));
        return std::nullopt;
    }

    const QByteArray trailingContent = currentStatement.trimmed();

    if(!trailingContent.isEmpty())
    {
        MigrationStatementSplitterUtils::setErrorMessage(errorMessage,QStringLiteral("Migration SQL Contains Content After the Final Statement Break Marker, or the Final Statement is Missing its Marker"));
        return std::nullopt;
    }

    if(statements.size() != markerCount)
    {
        MigrationStatementSplitterUtils::setErrorMessage(errorMessage,QStringLiteral("Migration Statement Count Does not Match Marker Count. Statements=%1, Markers=%2").arg(statements.size(),markerCount));
        return std::nullopt;
    }
    return statements;
}
