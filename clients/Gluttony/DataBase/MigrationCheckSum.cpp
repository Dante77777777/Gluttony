#include "MigrationCheckSum.h"

#include <QCryptographicHash>

namespace {
const QByteArray kUtf8Bom("\xEF\xBB\xBF",3);
}

MigrationCheckSumResult MigrationCheckSum::calculate(const QByteArray& sqlBytes)
{
    MigrationCheckSumResult result;
    result.normalizedSqlBytes = sqlBytes;

    // 1. 删除文件开头的 UTF-8 BOM。
    if(result.normalizedSqlBytes.startsWith(kUtf8Bom))
    {
        result.normalizedSqlBytes.remove(0,kUtf8Bom.size());
    }

    // 2. 先把 Windows CRLF 转换为 LF
    result.normalizedSqlBytes.replace("\r\n","\n");

    // 3. 再把剩余的单独 CR 转换为 LF。
    result.normalizedSqlBytes.replace('\r','\n');

    // 4. 对规范化后的完整迁移内容计算 SHA-256。
    const QByteArray digest = QCryptographicHash::hash(result.normalizedSqlBytes,QCryptographicHash::Sha256);

    // 5. 转换为64个小写十六进制字符。
    result.sha256Hex = QString::fromLatin1(digest.toHex());

    return result;
}


bool MigrationCheckSum::isValidSha256Hex(const QString& checksum)
{
    const qsizetype expectedLength = QCryptographicHash::hashLength(QCryptographicHash::Sha256)*2;
    if(checksum.size() != expectedLength)
    {
        return false;
    }

    for(const QChar character : checksum)
    {
        const ushort value = character.unicode();

        const bool isNumber = value >= static_cast<ushort>('0') && value <= static_cast<ushort>('9');

        const bool isLowercaseHexLetter = value >= static_cast<ushort>('a') && value <= static_cast<ushort>('f');

        if(!isNumber && !isLowercaseHexLetter)
        {
            return false;
        }
    }
    return true;
}
