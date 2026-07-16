#include "InferenceEngine.h"
#include <QFileInfo>

bool InferenceEngine::loadModel(const QString& xmlPath, QString* errorMessage)
{
    if(errorMessage != nullptr)
    {
        errorMessage->clear();
    }

    const QFileInfo fileInfo(xmlPath);

    if(!fileInfo.exists())
    {
        if(errorMessage != nullptr)
        {
            *errorMessage = "Model XML File Does Not Exist:\n" + xmlPath;
        }
        return false;
    }

    if(fileInfo.suffix().compare("xml",Qt::CaseInsensitive) != 0)
    {
        if(errorMessage != nullptr)
        {
            *errorMessage = "Please Select an OpenVINO XML Model file";
            //QFATAL("Please Select an OpenVINO XML Model file");
        }
        return false;
    }

    try
    {
        auto newModel = core.read_model(xmlPath.toStdString());
        auto newCompiledModel = core.compile_model(newModel,"CPU");
        model = newModel;
        compiledModel = newCompiledModel;
        loadedModelPath = fileInfo.absoluteFilePath();
        //QDEBUG("Model is Loaded Sucessfully!");
        return true;
    }
    catch (const std::exception& error)
    {
        if(errorMessage != nullptr)
        {
            *errorMessage = "OpenVINO Failed to Load the Model:\n" + QString::fromLocal8Bit(error.what());
            //QFATAL("OpenVINO Failed to Load the Model");
        }
        return false;
    }
}


bool InferenceEngine::isLoaded() const
{
    return !loadedModelPath.isEmpty();
}


QString InferenceEngine::modelPath() const
{
    return loadedModelPath;
}


QStringList InferenceEngine::inputDescriptions() const
{
    QStringList descriptions;
    if(!model)
    {
        return descriptions;
    }
    for(const auto& input : model->inputs())
    {
        const QString name = QString::fromStdString(input.get_any_name());
        const QString type = QString::fromStdString(input.get_element_type().to_string());
        const QString shape = QString::fromStdString(input.get_partial_shape().to_string());
        descriptions.append(QString("Input | Name: %1 | Type: %2 | Shape: %3").arg(name, type, shape));
    }
    return descriptions;
}


QStringList InferenceEngine::outputDescriptions() const
{
    QStringList descriptions;

    if (!model) {
        return descriptions;
    }

    for (const auto& output : model->outputs()) {
        const QString name = QString::fromStdString(output.get_any_name());
        const QString type = QString::fromStdString(
            output.get_element_type().to_string()
            );
        const QString shape = QString::fromStdString(
            output.get_partial_shape().to_string()
            );

        descriptions.append(
            QString("Output | Name: %1 | Type: %2 | Shape: %3")
                .arg(name, type, shape)
            );
    }

    return descriptions;
}
