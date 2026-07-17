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

struct InferenceResult
{
    bool isNg = false;
    float score = 0.0F;
    int inferenceMs = 0;

    QImage heatmapOverlay;
    int defectArea = 0;
};


class InferenceEngine
{
public:
    bool loadModel(const QString& xmlPath, QString* errorMessage = nullptr);
    bool isLoaded() const;
    bool inferImage(const QImage& image, InferenceResult* result, QString* errorMessage = nullptr);
    QString modelPath() const;

    QStringList inputDescriptions() const;

    QStringList outputDescriptions() const;
private:
    ov::Core core;
    std::shared_ptr<ov::Model> model;
    ov::CompiledModel compiledModel;
    QString loadedModelPath;
};









#endif
