#ifndef DATABASEPATHS_H
#define DATABASEPATHS_H

#include <QString>
#include <optional>

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

#include <utility>

class DataBasePaths final
{
public:
    [[nodiscard]]
    static std::optional<DataBasePaths> createDefault(QString *errorMessage = nullptr);

    [[nodiscard]]
    static std::optional<DataBasePaths> createUnderRoot(const QString& rootPath, QString *errorMessage = nullptr);

    [[nodiscard]]
    const QString& applicationDataRoot() const noexcept;

    [[nodiscard]]
    const QString& persistenceRoot() const noexcept;

    [[nodiscard]]
    const QString& dataBaseFilePath() const noexcept;

    [[nodiscard]]
    const QString& managedRunsRoot() const noexcept;

private:
    DataBasePaths(QString applicationDataRoot,
                  QString persistenceRoot,
                  QString databaseFilePath,
                  QString managedRunsRoot);
private:
    QString applicationDataRoot_;
    QString persistenceRoot_;
    QString dataBaseFilePath_;
    QString managedRunsRoot_;
};



#endif
