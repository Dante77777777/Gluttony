#include <QWidget>
#include <QBoxLayout>
#include <QFrame>
#include <QLabel>
#include <QPixmap>
#include "../Inference/InferenceEngine.h"


class QuickResWidgetUI
{
public:
    QVBoxLayout* labelsLayout;
    QHBoxLayout* resultLayout;
    QHBoxLayout* scoreLayout;
    QHBoxLayout* timeLayout;
    QHBoxLayout* areaLayout;
    QLabel* resultLabel;
    QLabel* resultValueLabel;
    QLabel* scoreLabel;
    QLabel* scoreValueLabel;
    QLabel* timeLabel;
    QLabel* areaLabel;

    void setupUI(QWidget* parent)
    {
        parent->setObjectName("QuickResWidget");
        labelsLayout = new QVBoxLayout(parent);
        labelsLayout->setContentsMargins(12, 12, 12, 12);
        labelsLayout->setSpacing(10);

        resultLabel = new QLabel("Result:");
        resultLabel->setObjectName("QuickResultTitle");
        resultValueLabel = new QLabel("");
        resultValueLabel->setObjectName("QuickResultValue");
        resultLayout = new QHBoxLayout();
        resultLayout->addWidget(resultLabel);
        resultLayout->addWidget(resultValueLabel);
        labelsLayout->addLayout(resultLayout);

        scoreLabel = new QLabel("Score:");
        scoreLabel->setObjectName("QuickMetricTitle");
        scoreValueLabel = new QLabel("0");
        scoreValueLabel->setObjectName("QuickScoreValue");
        auto* scoreIcon = new QLabel();
        scoreIcon->setFixedSize(22, 22);
        scoreIcon->setPixmap(QPixmap(":/score.png").scaled(
            18, 18, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        scoreLayout = new QHBoxLayout();
        scoreLayout->setContentsMargins(0, 0, 0, 0);
        scoreLayout->setSpacing(6);
        scoreLayout->addWidget(scoreIcon);
        scoreLayout->addWidget(scoreLabel);
        scoreLayout->addWidget(scoreValueLabel);
        scoreLayout->addStretch();
        labelsLayout->addLayout(scoreLayout);

        auto* scoreSeparator = new QFrame();
        scoreSeparator->setObjectName("QuickSectionSeparator");
        scoreSeparator->setFrameShape(QFrame::HLine);
        labelsLayout->addWidget(scoreSeparator);

        timeLabel = new QLabel("Inference: ms");
        timeLabel->setObjectName("QuickInference");
        auto* timeIcon = new QLabel();
        timeIcon->setFixedSize(22, 22);
        timeIcon->setPixmap(QPixmap(":/inference.png").scaled(
            18, 18, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        timeLayout = new QHBoxLayout();
        timeLayout->setContentsMargins(0, 0, 0, 0);
        timeLayout->setSpacing(6);
        timeLayout->addWidget(timeIcon);
        timeLayout->addWidget(timeLabel, 1);
        labelsLayout->addLayout(timeLayout);

        areaLabel = new QLabel("Defect Area: px");
        areaLabel->setObjectName("QuickDefectArea");
        auto* areaIcon = new QLabel();
        areaIcon->setFixedSize(22, 22);
        areaIcon->setPixmap(QPixmap(":/defect-area.png").scaled(
            18, 18, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        areaLayout = new QHBoxLayout();
        areaLayout->setContentsMargins(0, 0, 0, 0);
        areaLayout->setSpacing(6);
        areaLayout->addWidget(areaIcon);
        areaLayout->addWidget(areaLabel, 1);
        labelsLayout->addLayout(areaLayout);

    }

    void setNG(DetectionLevel level,double score)
    {
        QString color;
        QString resultText;

        switch (level) {
        case DetectionLevel::OK:
            resultText = "OK";
            color = "#16a34a";
            break;

        case DetectionLevel::REVIEW:
            resultText = "REVIEW";
            color = "#d97706";
            break;

        case DetectionLevel::NG:
            resultText = "NG";
            color = "#dc2626";
            break;
        }

        resultValueLabel->setText(resultText);
        resultValueLabel->setStyleSheet(
            QString("color: %1; font-size: 28px; font-weight: 800;")
                .arg(color)
            );

        scoreValueLabel->setText(QString::number(score, 'f', 3));
        scoreValueLabel->setStyleSheet(
            QString("color: %1; font-size: 20px; font-weight: 800;")
                .arg(color)
            );
    }

    void setMetrics(int inferenceMs, int defectArea)
    {
        timeLabel->setText(
            QString("Inference: %1 ms").arg(inferenceMs));

        areaLabel->setText(
            QString("Defect Area: %1 px").arg(defectArea));
    }
};
