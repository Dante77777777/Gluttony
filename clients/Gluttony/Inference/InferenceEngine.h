#ifndef INFERENCEENGINE_H
#define INFERENCEENGINE_H

#include <QString>
#include <QStringList>
#include <QImage>
#include <QElapsedTimer>
#include <cstdint>
#include <vector>
#include "openvino/openvino.hpp"
#include "opencv2/opencv.hpp"
#include "../LogWidget/LogMessage.h"

enum class DetectionLevel
{
    OK,
    REVIEW,
    NG
};

struct InferenceOptions
{
    double reviewThreshold = 0.45;
    double ngThreshold = 0.60;
    double heatmapAlpha = 0.55;
};

struct InferenceResult
{
    DetectionLevel detectionLevel = DetectionLevel::OK;
    float score = 0.0F;
    int inferenceMs = 0;

    QImage heatmapOverlay;
    int defectArea = 0;
};

Q_DECLARE_METATYPE(InferenceResult)

class InferenceEngine
{
public:
    bool loadModel(const QString& modelPath, QString* errorMessage = nullptr);
    bool isLoaded() const;
    bool inferImage(const QImage& image, const InferenceOptions& options, InferenceResult* result, QString* errorMessage = nullptr);
    QString modelPath() const;

    QStringList inputDescriptions() const;

    QStringList outputDescriptions() const;
private:
    ov::Core core;
    std::shared_ptr<ov::Model> model;
    ov::CompiledModel compiledModel;
    QString loadedModelPath;
    int inputWidth = 0;
    int inputHeight = 0;
};









#endif
