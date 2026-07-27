#include "DataBase/DatabaseBootstrapper.h"
#include "DataBase/MigrationCatalog.h"
#include "DataBase/PersistenceWorker.h"

#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QSqlDatabase>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>
#include <QUuid>

#include <cstdio>

namespace
{

bool waitForLoop(QEventLoop& loop, int timeoutMs = 5000)
{
    bool timedOut = false;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(
        &timer,
        &QTimer::timeout,
        &loop,
        [&]()
        {
            timedOut = true;
            loop.quit();
        });
    timer.start(timeoutMs);
    loop.exec();
    return !timedOut;
}

int fail(const QString& message)
{
    std::fprintf(stderr, "%s\n", qPrintable(message));
    std::fflush(stderr);
    return 1;
}

class WorkerThreadGuard final
{
public:
    WorkerThreadGuard(QThread* thread, PersistenceWorker* worker)
        : thread_(thread)
        , worker_(worker)
    {
    }

    ~WorkerThreadGuard()
    {
        stop();
    }

    void stop()
    {
        if (thread_ == nullptr || !thread_->isRunning())
        {
            thread_ = nullptr;
            worker_ = nullptr;
            return;
        }

        if (worker_ != nullptr)
        {
            QMetaObject::invokeMethod(
                worker_,
                [worker = worker_]()
                {
                    worker->shutdown();
                },
                Qt::BlockingQueuedConnection);
        }
        thread_->quit();
        thread_->wait();
        thread_ = nullptr;
        worker_ = nullptr;
    }

private:
    QThread* thread_ = nullptr;
    PersistenceWorker* worker_ = nullptr;
};

}

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);
    QTemporaryDir temporaryDirectory;
    if (!temporaryDirectory.isValid())
    {
        return fail(QStringLiteral("Failed to Create Temporary Directory."));
    }

    const QString persistenceRoot =
        QDir(temporaryDirectory.path()).filePath(QStringLiteral("persistence"));
    const QString managedRunsRoot =
        QDir(persistenceRoot).filePath(QStringLiteral("runs"));
    if (!QDir().mkpath(managedRunsRoot))
    {
        return fail(QStringLiteral("Failed to Create Managed Runs Root."));
    }

    const QString dataBaseFilePath =
        QDir(persistenceRoot).filePath(QStringLiteral("gluttony.db"));
    QString errorMessage;
    const auto migrations =
        MigrationCatalog::loadEmbeddedMigrations(&errorMessage);
    if (!migrations.has_value())
    {
        return fail(errorMessage);
    }
    const auto bootstrap = DatabaseBootstrapper::initialize(
        dataBaseFilePath,
        QStringLiteral("persistence.worker.smoke.bootstrap"),
        migrations.value(),
        &errorMessage);
    if (!bootstrap.has_value())
    {
        return fail(errorMessage);
    }

    const QString sourcePath =
        QDir(temporaryDirectory.path()).filePath(QStringLiteral("source.png"));
    QImage sourceImage(8, 8, QImage::Format_RGB32);
    sourceImage.fill(Qt::blue);
    if (!sourceImage.save(sourcePath, "PNG"))
    {
        return fail(QStringLiteral("Failed to Create Source PNG."));
    }

    const QString modelPath =
        QDir(temporaryDirectory.path()).filePath(QStringLiteral("model.onnx"));
    QFile modelFile(modelPath);
    if (!modelFile.open(QIODevice::WriteOnly)
        || modelFile.write("worker-smoke-model") <= 0)
    {
        return fail(QStringLiteral("Failed to Create Model Fixture."));
    }
    modelFile.close();

    const QString connectionName =
        QStringLiteral("persistence.worker.smoke.connection");
    auto* worker = new PersistenceWorker(
        dataBaseFilePath,
        managedRunsRoot,
        connectionName);
    QThread thread;
    worker->moveToThread(&thread);
    QObject::connect(
        &thread,
        &QThread::started,
        worker,
        &PersistenceWorker::initialize);
    QObject::connect(
        &thread,
        &QThread::finished,
        worker,
        &QObject::deleteLater);
    WorkerThreadGuard threadGuard(&thread, worker);

    QString operationFailure;
    QObject::connect(
        worker,
        &PersistenceWorker::operationFailed,
        &application,
        [&](const QString& operation,
            const QString& context,
            const QString& failure)
        {
            operationFailure =
                QStringLiteral("%1 | %2 | %3")
                    .arg(operation, context, failure);
        });

    bool initialized = false;
    QEventLoop initializeLoop;
    QObject::connect(
        worker,
        &PersistenceWorker::initialized,
        &initializeLoop,
        [&](bool success, const QString& failure)
        {
            initialized = success;
            errorMessage = failure;
            initializeLoop.quit();
        });
    thread.start();
    if (!waitForLoop(initializeLoop) || !initialized)
    {
        thread.quit();
        thread.wait();
        return fail(
            errorMessage.isEmpty()
                ? QStringLiteral("Persistence Worker Initialization Timed Out.")
                : errorMessage);
    }

    NewInspectionRun newRun;
    newRun.runId =
        QUuid::createUuid().toString(QUuid::WithoutBraces);
    newRun.sourceType = QStringLiteral("SINGLE_IMAGE");
    newRun.sourcePath = sourcePath;
    newRun.storagePath = QDir(managedRunsRoot).filePath(newRun.runId);
    newRun.storageOrigin = QStringLiteral("MANAGED");
    newRun.modelPath = modelPath;
    newRun.appVersion = QStringLiteral("worker-smoke");
    newRun.reviewThreshold = 0.45;
    newRun.ngThreshold = 0.60;
    newRun.heatmapAlpha = 0.55;
    newRun.startedAtMs = 1'700'000'000'000LL;

    InspectionRunRecord createdRun;
    QEventLoop createLoop;
    const QMetaObject::Connection createConnection = QObject::connect(
        worker,
        &PersistenceWorker::runCreated,
        &createLoop,
        [&](const InspectionRunRecord& run)
        {
            createdRun = run;
            createLoop.quit();
        });
    QMetaObject::invokeMethod(
        worker,
        [worker, newRun]()
        {
            worker->createRun(newRun);
        },
        Qt::QueuedConnection);
    if (!waitForLoop(createLoop))
    {
        return fail(
            operationFailure.isEmpty()
                ? QStringLiteral("Run Creation Timed Out.")
                : operationFailure);
    }
    QObject::disconnect(createConnection);
    if (createdRun.totalCount != 1
        || createdRun.status != QStringLiteral("RUNNING"))
    {
        return fail(QStringLiteral("Created Run Metadata is Incorrect."));
    }

    ClaimedInspectionItem claimedItem;
    QEventLoop claimLoop;
    const QMetaObject::Connection claimConnection = QObject::connect(
        worker,
        &PersistenceWorker::itemClaimed,
        &claimLoop,
        [&](const ClaimedInspectionItem& item)
        {
            claimedItem = item;
            claimLoop.quit();
        });
    QMetaObject::invokeMethod(
        worker,
        [worker, runId = newRun.runId]()
        {
            worker->claimNext(runId);
        },
        Qt::QueuedConnection);
    if (!waitForLoop(claimLoop) || !claimedItem.found)
    {
        return fail(
            operationFailure.isEmpty()
                ? QStringLiteral("Claiming the Input Item Failed.")
                : operationFailure);
    }
    QObject::disconnect(claimConnection);

    InferenceResult inferenceResult;
    inferenceResult.detectionLevel = DetectionLevel::REVIEW;
    inferenceResult.score = 0.52F;
    inferenceResult.inferenceMs = 12;
    inferenceResult.defectArea = 4;
    inferenceResult.heatmapOverlay =
        QImage(8, 8, QImage::Format_RGB32);
    inferenceResult.heatmapOverlay.fill(Qt::red);

    PersistedInspectionResult persistedResult;
    QEventLoop persistLoop;
    const QMetaObject::Connection persistConnection = QObject::connect(
        worker,
        &PersistenceWorker::successPersisted,
        &persistLoop,
        [&](const PersistedInspectionResult& result)
        {
            persistedResult = result;
            persistLoop.quit();
        });
    QMetaObject::invokeMethod(
        worker,
        [worker, claimedItem, inferenceResult]()
        {
            worker->persistSuccess(claimedItem, inferenceResult);
        },
        Qt::QueuedConnection);
    if (!waitForLoop(persistLoop)
        || !QFileInfo::exists(persistedResult.heatmapFilePath))
    {
        return fail(
            operationFailure.isEmpty()
                ? QStringLiteral("Persisting the Successful Result Failed.")
                : operationFailure);
    }
    QObject::disconnect(persistConnection);

    ClaimedInspectionItem emptyClaim;
    QEventLoop emptyClaimLoop;
    const QMetaObject::Connection emptyClaimConnection = QObject::connect(
        worker,
        &PersistenceWorker::itemClaimed,
        &emptyClaimLoop,
        [&](const ClaimedInspectionItem& item)
        {
            emptyClaim = item;
            emptyClaimLoop.quit();
        });
    QMetaObject::invokeMethod(
        worker,
        [worker, runId = newRun.runId]()
        {
            worker->claimNext(runId);
        },
        Qt::QueuedConnection);
    if (!waitForLoop(emptyClaimLoop)
        || emptyClaim.found
        || emptyClaim.runId != newRun.runId)
    {
        return fail(
            operationFailure.isEmpty()
                ? QStringLiteral(
                      "Empty Claim Did not Preserve its Run Identity.")
                : operationFailure);
    }
    QObject::disconnect(emptyClaimConnection);

    InspectionRunRecord draftRun;
    QEventLoop draftLoop;
    const QMetaObject::Connection draftConnection = QObject::connect(
        worker,
        &PersistenceWorker::runDrafted,
        &draftLoop,
        [&](const InspectionRunRecord& run)
        {
            draftRun = run;
            draftLoop.quit();
        });
    QMetaObject::invokeMethod(
        worker,
        [worker, runId = newRun.runId]()
        {
            worker->finishAsDraft(runId);
        },
        Qt::QueuedConnection);
    if (!waitForLoop(draftLoop)
        || draftRun.status != QStringLiteral("DRAFT"))
    {
        return fail(
            operationFailure.isEmpty()
                ? QStringLiteral("Finishing the Run as Draft Failed.")
                : operationFailure);
    }
    QObject::disconnect(draftConnection);

    bool saved = false;
    QEventLoop saveLoop;
    const QMetaObject::Connection saveConnection = QObject::connect(
        worker,
        &PersistenceWorker::runSaved,
        &saveLoop,
        [&](const QString& runId)
        {
            saved = runId == newRun.runId;
            saveLoop.quit();
        });
    QMetaObject::invokeMethod(
        worker,
        [worker, runId = newRun.runId]()
        {
            worker->saveDraft(
                runId,
                QStringLiteral("Worker Smoke"),
                QStringLiteral("Disk-backed result test"));
        },
        Qt::QueuedConnection);
    if (!waitForLoop(saveLoop)
        || !saved
        || !QFileInfo::exists(
            QDir(newRun.storagePath).filePath(
                QStringLiteral("manifest.json")))
        || !QFileInfo::exists(
            QDir(newRun.storagePath).filePath(
                QStringLiteral("items.json"))))
    {
        return fail(
            operationFailure.isEmpty()
                ? QStringLiteral("Saving the Draft Failed.")
                : operationFailure);
    }
    QObject::disconnect(saveConnection);

    InspectionRunRecords catalog;
    QEventLoop catalogLoop;
    const QMetaObject::Connection catalogConnection = QObject::connect(
        worker,
        &PersistenceWorker::batchCatalogReady,
        &catalogLoop,
        [&](const InspectionRunRecords& runs)
        {
            catalog = runs;
            catalogLoop.quit();
        });
    QMetaObject::invokeMethod(
        worker,
        [worker]()
        {
            worker->requestBatchCatalog();
        },
        Qt::QueuedConnection);
    if (!waitForLoop(catalogLoop)
        || catalog.size() != 1
        || catalog.first().runId != newRun.runId
        || catalog.first().status != QStringLiteral("SAVED"))
    {
        return fail(
            operationFailure.isEmpty()
                ? QStringLiteral("Reading the Batch Catalog Failed.")
                : operationFailure);
    }
    QObject::disconnect(catalogConnection);

    InspectionRunRecord loadedRun;
    InspectionItemRows loadedItems;
    QEventLoop loadLoop;
    const QMetaObject::Connection loadConnection = QObject::connect(
        worker,
        &PersistenceWorker::runLoaded,
        &loadLoop,
        [&](const InspectionRunRecord& run,
            const InspectionItemRows& items,
            bool resumed)
        {
            if (!resumed)
            {
                loadedRun = run;
                loadedItems = items;
                loadLoop.quit();
            }
        });
    QMetaObject::invokeMethod(
        worker,
        [worker, runId = newRun.runId]()
        {
            worker->loadSavedRun(runId);
        },
        Qt::QueuedConnection);
    if (!waitForLoop(loadLoop)
        || loadedRun.status != QStringLiteral("SAVED")
        || loadedItems.size() != 1
        || !loadedItems.first().heatmapRelativePath.has_value())
    {
        return fail(
            operationFailure.isEmpty()
                ? QStringLiteral("Loading the Saved Run Failed.")
                : operationFailure);
    }
    QObject::disconnect(loadConnection);

    bool deleted = false;
    QEventLoop deleteLoop;
    QObject::connect(
        worker,
        &PersistenceWorker::savedRunDeleted,
        &deleteLoop,
        [&](const QString& runId)
        {
            deleted = runId == newRun.runId;
            deleteLoop.quit();
        });
    QMetaObject::invokeMethod(
        worker,
        [worker, runId = newRun.runId]()
        {
            worker->deleteSavedRun(runId);
        },
        Qt::QueuedConnection);
    if (!waitForLoop(deleteLoop)
        || !deleted
        || QFileInfo::exists(newRun.storagePath))
    {
        return fail(
            operationFailure.isEmpty()
                ? QStringLiteral("Deleting the Saved Run Failed.")
                : operationFailure);
    }

    threadGuard.stop();

    if (QSqlDatabase::contains(connectionName))
    {
        return fail(
            QStringLiteral("Persistence Connection Was not Removed."));
    }

    return 0;
}
