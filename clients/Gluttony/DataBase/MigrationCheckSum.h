#ifndef MIGRATIONCHECKSUM_H
#define MIGRATIONCHECKSUM_H

#include <QByteArray>
#include <QString>
#include <QCryptographicHash>

struct MigrationCheckSumResult final
{
    QByteArray normalizedSqlBytes;
    QString sha256Hex;
};

class MigrationCheckSum final
{
public:
    [[nodiscard]]
    static MigrationCheckSumResult calculate(const QByteArray& sqlBytes);

    [[nodiscard]]
    static bool isValidSha256Hex(const QString& checksum);
};


#endif
