#include <QStatusBar>
#include <QLabel>
#include <QFrame>
#include <QHBoxLayout>

class StatusBarUI
{
public:
    QStatusBar* statusBar;
    QLabel* readyLabel;
    QWidget* midWidget;
    QHBoxLayout* midLayout;
    QLabel* imageCountLabel;
    QLabel* ngLabel;
    QLabel* okLabel;
    QLabel* avgScoreLabel;
    QLabel* cpuLabel;

    void setupUI(QWidget* parent = nullptr)
    {
        auto* separator1 = new QFrame();
        separator1->setObjectName("StatusBarSeparator");
        separator1->setFrameShape(QFrame::VLine);
        separator1->setFrameShadow(QFrame::Sunken);
        separator1->setFixedHeight(18);

        auto* separator2 = new QFrame();
        separator2->setObjectName("StatusBarSeparator");
        separator2->setFrameShape(QFrame::VLine);
        separator2->setFrameShadow(QFrame::Sunken);
        separator2->setFixedHeight(18);

        auto* separator3 = new QFrame();
        separator3->setObjectName("StatusBarSeparator");
        separator3->setFrameShape(QFrame::VLine);
        separator3->setFrameShadow(QFrame::Sunken);
        separator3->setFixedHeight(18);

        statusBar = new QStatusBar(parent);
        midWidget = new QWidget(statusBar);
        midLayout = new QHBoxLayout(midWidget);
        midLayout->setContentsMargins(12, 0, 12, 0);
        midLayout->setSpacing(12);

        readyLabel = new QLabel("Ready");
        readyLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

        imageCountLabel = new QLabel("Images:0");
        ngLabel = new QLabel("NG:0");
        ngLabel->setObjectName("FooterNg");
        okLabel = new QLabel("OK:0");
        okLabel->setObjectName("FooterOk");
        avgScoreLabel = new QLabel("Avg Score:0.000");
        cpuLabel = new QLabel("● CPU");

        midLayout->addWidget(readyLabel);
        midLayout->addStretch();

        midLayout->addWidget(imageCountLabel);
        midLayout->addWidget(separator1);
        midLayout->addWidget(ngLabel);
        midLayout->addWidget(separator2);
        midLayout->addWidget(okLabel);
        midLayout->addWidget(separator3);
        midLayout->addWidget(avgScoreLabel);

        midLayout->addStretch();

        midLayout->addWidget(cpuLabel);

        statusBar->addWidget(midWidget,1);
        midLayout->setContentsMargins(12, 0, 12, 0);
        midLayout->setSpacing(12);
    }
};
