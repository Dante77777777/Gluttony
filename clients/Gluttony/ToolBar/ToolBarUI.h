#include <QToolBar>
#include <QStyle>
#include <QAction>
#include <QSize>
#include <QIcon>
#include <QWidget>

class ToolBarUI
{
public:
    QToolBar* topToolBar;
    QAction* openImageAction;
    QAction* openFolderAction;
    QAction* startAction;
    QAction* stopAction;
    QAction* saveBatchAction;
    QAction* discardBatchAction;
    QAction* batchManagerAction;
    QAction* cameraTcpAction;
    QAction* zoomInAction;
    QAction* zoomOutAction;
    QAction* fitAction;
    QAction* rotateLeftAction;
    QAction* rotateRightAction;
    QAction* settingAction;
    QAction* aboutAction;

    void setupUI(QWidget* parent)
    {
        topToolBar = new QToolBar("Top Toolbar",parent);
        topToolBar->setObjectName("TopToolBar");
        topToolBar->setMovable(false);
        topToolBar->setFloatable(false);
        topToolBar->setIconSize(QSize(22,22));
        topToolBar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);

        openImageAction = new QAction(QIcon(":/file-image.png"),"Open Image",parent);
        openFolderAction = new QAction(QIcon(":/folder-open.png"),"Open Folder",parent);
        startAction = new QAction(QIcon(":/play.png"),"Start",parent);
        startAction->setEnabled(false);
        stopAction = new QAction(QIcon(":/stop.png"),"Stop",parent);
        stopAction->setEnabled(false);
        saveBatchAction =
            new QAction(QIcon(":/save.png"), "Save Batch", parent);
        saveBatchAction->setEnabled(false);
        discardBatchAction =
            new QAction(QIcon(":/stop.png"), "Discard Draft", parent);
        discardBatchAction->setEnabled(false);
        batchManagerAction =
            new QAction(QIcon(":/refer.png"), "Batches", parent);
        batchManagerAction->setEnabled(false);
        cameraTcpAction = new QAction(
            parent->style()->standardIcon(
                QStyle::SP_DriveNetIcon),
            "TCP Off",
            parent);
        cameraTcpAction->setObjectName(
            "CameraTcpAction");
        cameraTcpAction->setCheckable(true);
        cameraTcpAction->setChecked(false);
        cameraTcpAction->setToolTip(
            "Enable or disable the industrial camera TCP service");
        zoomInAction = new QAction(QIcon(":/zoom in.png"),"Zoom In",parent);
        zoomOutAction = new QAction(QIcon(":/zoom out.png"),"Zoom Out",parent);
        fitAction = new QAction(QIcon(":/scale-up.png"),"Fit",parent);
        rotateLeftAction = new QAction(QIcon(":/rotate-left.png"),"Rotate Left",parent);
        rotateRightAction = new QAction(QIcon(":/rotate-right.png"),"RotateRight",parent);
        settingAction = new QAction(QIcon(":/setting.png"),"Setting",parent);
        aboutAction = new QAction(QIcon(":/about.png"),"About",parent);

        topToolBar->addAction(openImageAction);
        topToolBar->addAction(openFolderAction);
        topToolBar->addSeparator();

        topToolBar->addAction(startAction);
        topToolBar->addAction(stopAction);
        topToolBar->addSeparator();

        topToolBar->addAction(saveBatchAction);
        topToolBar->addAction(discardBatchAction);
        topToolBar->addAction(batchManagerAction);
        topToolBar->addSeparator();

        topToolBar->addAction(cameraTcpAction);
        topToolBar->addSeparator();

        topToolBar->addAction(zoomInAction);
        topToolBar->addAction(zoomOutAction);
        topToolBar->addSeparator();

        topToolBar->addAction(fitAction);
        topToolBar->addAction(rotateLeftAction);
        topToolBar->addAction(rotateRightAction);
        topToolBar->addSeparator();

        topToolBar->addAction(settingAction);
        topToolBar->addAction(aboutAction);
        topToolBar->addSeparator();
    }
};
