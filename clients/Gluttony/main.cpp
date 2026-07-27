#include "mainwindow.h"
#include "LogWidget/LogMessage.h"
#include "DataBase/DataBasePaths.h"
#include "DataBase/MigrationCatalog.h"
#include "DataBase/DatabaseBootstrapper.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>

void applyQSS()
{
    QFile file(":/style.qss");
    if (file.open(QFile::ReadOnly)) {
        QString styleSheet = QLatin1String(file.readAll());
        qApp->setStyleSheet(styleSheet); // 为整个应用程序设置
        file.close();
    }
}

int main(int argc, char *argv[])
{
    // 1. 初始化 Qt 应用、日志系统与稳定的应用名称。
    QApplication a(argc, argv);
    LogMessage::getInstance().installMessageHandler();
    QCoreApplication::setApplicationName(
        QStringLiteral("Gluttony"));
    QCoreApplication::setApplicationVersion(
        QStringLiteral("0.2.0"));

    // 2. 确定数据库文件和受管理批次目录，并确保基础目录存在。
    QString DBErrorMessage;

    const auto dataBasePaths = DataBasePaths::createDefault(&DBErrorMessage);
    const QString bootstrapConnectionName = QStringLiteral("gluttony.bootstrap");

    if(!dataBasePaths.has_value())
    {
        QCRITICAL(QString("Persistence Path Initialization Failed: %1").arg(DBErrorMessage));
        return 1;
    }

    // 3. 从 Qt 资源系统读取内嵌迁移目录。
    // 当前阶段只验证迁移资源存在且内容非空，不执行任何迁移 SQL。
    QString migrationResourceError;

    const auto embeddedMigrations =
        MigrationCatalog::
        loadEmbeddedMigrations(
            &migrationResourceError);

    if (!embeddedMigrations.has_value())
    {
        QCRITICAL(
            QString(
                "Embedded Migration Loading Failed: %1")
                .arg(
                    migrationResourceError));

        return 1;
    }

    if (embeddedMigrations->isEmpty())
    {
        QCRITICAL(
            QStringLiteral(
                "Embedded Migration Catalog is Empty."));

        return 1;
    }

    for (const MigrationSource &migration
         : embeddedMigrations.value())
    {
        QDEBUG(
            QString(
                "Embedded Migration Loaded: "
                "Version=%1, "
                "Name=%2, "
                "Resource=%3, "
                "Bytes=%4, "
                "Statements=%5, "
                "ChecksumSha256=%6")
                .arg(migration.version)
                .arg(migration.name)
                .arg(migration.resourcePath)
                .arg(migration.sqlBytes.size())
                .arg(migration.statements.size())
                .arg(migration.checksumSha256));
    }

    // 4. 打开启动连接、配置 SQLite、执行待处理迁移并验证最终版本。
    QString bootstrapErrorMessage;
    const auto bootstrapReport =
        DatabaseBootstrapper::initialize(
            dataBasePaths->dataBaseFilePath(),
            bootstrapConnectionName,
            embeddedMigrations.value(),
            &bootstrapErrorMessage);

    if (!bootstrapReport.has_value()) {
        QCRITICAL(
            QString(
                "Database Bootstrap Failed: %1")
                .arg(bootstrapErrorMessage));
        return 1;
    }

    QDEBUG(
        QString(
            "Database Bootstrap Completed: "
            "SQLiteVersion=%1, "
            "ApplicationId=%2, "
            "InitialUserVersion=%3, "
            "FinalUserVersion=%4, "
            "PreviouslyApplied=%5, "
            "AppliedThisStartup=%6, "
            "TotalApplied=%7, "
            "RecoveredProcessingItems=%8, "
            "InterruptedRuns=%9")
            .arg(bootstrapReport->sqliteVersion.toString())
            .arg(bootstrapReport->applicationId)
            .arg(bootstrapReport->initialUserVersion)
            .arg(bootstrapReport->finalUserVersion)
            .arg(bootstrapReport->previouslyAppliedCount)
            .arg(bootstrapReport->appliedThisStartupCount)
            .arg(bootstrapReport->totalAppliedCount)
            .arg(bootstrapReport->recoveredProcessingItemCount)
            .arg(bootstrapReport->interruptedRunCount));

    for (const MigrationExecutionResult& result
         : bootstrapReport->appliedMigrations) {
        QDEBUG(
            QString(
                "Migration Applied: "
                "Version=%1, "
                "Name=%2, "
                "Statements=%3, "
                "AppliedAtMs=%4")
                .arg(result.version)
                .arg(result.name)
                .arg(result.executedStatementCount)
                .arg(result.appliedAtMs));
    }

    QDEBUG(
        QString(
            "SQLite Connection Closed and Removed: %1")
            .arg(bootstrapConnectionName));


    QDEBUG(QString("Application Data Root: %1").arg(QDir::toNativeSeparators(dataBasePaths->applicationDataRoot())));

    QDEBUG(QString("Persistence Root: %1").arg(QDir::toNativeSeparators(dataBasePaths->persistenceRoot())));

    QDEBUG(QString("Database File: %1").arg(QDir::toNativeSeparators(dataBasePaths->dataBaseFilePath())));

    QDEBUG(QString("Managed Runs Root: %1").arg(QDir::toNativeSeparators(dataBasePaths->managedRunsRoot())));

    // 6. 数据库启动检查完成后再创建并显示主窗口。
    qRegisterMetaType<InferenceResult>("InferenceResult");
    qRegisterMetaType<ClaimedInspectionItem>("ClaimedInspectionItem");
    qRegisterMetaType<InspectionItemPageRow>("InspectionItemPageRow");
    qRegisterMetaType<InspectionItemRows>("InspectionItemRows");
    qRegisterMetaType<InspectionRunRecord>("InspectionRunRecord");
    qRegisterMetaType<InspectionRunRecords>("InspectionRunRecords");
    qRegisterMetaType<PersistedInspectionResult>(
        "PersistedInspectionResult");

    MainWindow w(
        dataBasePaths->dataBaseFilePath(),
        dataBasePaths->managedRunsRoot());
    applyQSS();
    w.show();
    return a.exec();
}
