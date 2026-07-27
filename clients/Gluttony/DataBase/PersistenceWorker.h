#ifndef PERSISTENCEWORKER_H
#define PERSISTENCEWORKER_H

#include "DataBaseConnection.h"
#include "PersistenceRecords.h"

#include "../Inference/InferenceEngine.h"

#include <QObject>
#include <QSqlDatabase>

#include <optional>

class PersistenceWorker final : public QObject
{
    Q_OBJECT

public:
    explicit PersistenceWorker(
        QString dataBaseFilePath,
        QString managedRunsRoot,
        QString connectionName,
        QObject* parent = nullptr);

public slots:
    void initialize();
    void shutdown();

    void createRun(NewInspectionRun run);
    void claimNext(const QString& runId);
    void persistSuccess(
        const ClaimedInspectionItem& item,
        const InferenceResult& result);
    void persistFailure(
        const ClaimedInspectionItem& item,
        const QString& errorMessage);
    void finishAsDraft(const QString& runId);
    void saveDraft(
        const QString& runId,
        const QString& name,
        const QString& note);
    void discardDraft(const QString& runId);
    void requestBatchCatalog();
    void loadSavedRun(const QString& runId);
    void resumeInterruptedRun(const QString& runId);
    void deleteSavedRun(const QString& runId);
    void interruptRunningRun(const QString& runId);

signals:
    void initialized(bool success, const QString& errorMessage);
    void interruptedRunsReady(const InspectionRunRecords& runs);

    void runCreated(const InspectionRunRecord& run);
    void itemClaimed(const ClaimedInspectionItem& item);
    void successPersisted(const PersistedInspectionResult& result);
    void failurePersisted(const InspectionItemPageRow& item);
    void runDrafted(const InspectionRunRecord& run);
    void runSaved(const QString& runId);
    void runDiscarded(const QString& runId);
    void batchCatalogReady(const InspectionRunRecords& runs);
    void runLoaded(
        const InspectionRunRecord& run,
        const InspectionItemRows& items,
        bool resumed);
    void savedRunDeleted(const QString& runId);
    void runInterrupted(const QString& runId);
    void operationFailed(
        const QString& operation,
        const QString& contextId,
        const QString& errorMessage);

private:
    [[nodiscard]]
    bool ensureReady(const QString& operation, const QString& contextId);

    [[nodiscard]]
    QSqlDatabase dataBase() const;

    [[nodiscard]]
    std::optional<InspectionRunRecord> readRun(
        const QSqlDatabase& dataBase,
        const QString& runId,
        QString* errorMessage) const;

    [[nodiscard]]
    std::optional<InspectionItemRows> readAllItems(
        const QSqlDatabase& dataBase,
        const QString& runId,
        QString* errorMessage) const;

    [[nodiscard]]
    std::optional<InspectionRunRecords> readBatchCatalog(
        const QSqlDatabase& dataBase,
        QString* errorMessage) const;

    [[nodiscard]]
    bool createRunFromSource(
        QSqlDatabase& dataBase,
        NewInspectionRun* run,
        qint64* totalCount,
        QString* errorMessage);

    [[nodiscard]]
    bool writeRunManifest(
        const QSqlDatabase& dataBase,
        const InspectionRunRecord& run,
        SaveInspectionRunRequest* request,
        QString* errorMessage) const;

    [[nodiscard]]
    bool interruptRunInDataBase(
        QSqlDatabase& dataBase,
        const QString& runId,
        qint64 interruptedAtMs,
        QString* errorMessage);

    void discardStartupDrafts(QSqlDatabase& dataBase);
    void processCleanupJobs(QSqlDatabase& dataBase);

    [[nodiscard]]
    bool isManagedRunPath(const QString& path) const;

    QString dataBaseFilePath_;
    QString managedRunsRoot_;
    QString connectionName_;
    std::optional<DataBaseConnection> connection_;
    bool ready_ = false;
};

#endif
