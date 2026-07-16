#ifndef INFERENCEENGINE_H
#define INFERENCEENGINE_H

#include <QString>
#include <QStringList>
#include "openvino/openvino.hpp"
#include "../LogWidget/LogMessage.h"


class InferenceEngine
{
public:
    bool loadModel(const QString& xmlPath, QString* errorMessage = nullptr);
    bool isLoaded() const;
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
