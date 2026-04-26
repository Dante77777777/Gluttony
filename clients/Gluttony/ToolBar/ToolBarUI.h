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
        startAction = new QAction(QIcon(":/play.png"),"Strat",parent);
        stopAction = new QAction(QIcon(":/stop.png"),"Stop",parent);
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
