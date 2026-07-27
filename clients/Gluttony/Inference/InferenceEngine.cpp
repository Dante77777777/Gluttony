#include "InferenceEngine.h"
#include <algorithm>
#include <QFileInfo>

namespace AssistFunc{
QImage createHeatmapOverlay(const QImage& sourceImage, const float* anomalyData, int mapWidth, int mapHeight, float alpha)
{
    if(sourceImage.isNull() || anomalyData == nullptr)
    {
        return {};
    }
    cv::Mat anomalyMap(mapHeight,mapWidth,CV_32FC1);
    for(int y = 0; y < mapHeight; ++y)
    {
        for(int x = 0; x < mapWidth; ++x)
        {
            anomalyMap.at<float>(y,x) = anomalyData[y*mapWidth+x];
        }
    }

    cv::Mat normalizedMap;
    cv::normalize(anomalyMap,normalizedMap,0,255,cv::NORM_MINMAX,CV_8UC1);

    cv::Mat colorMap;
    cv::applyColorMap(normalizedMap, colorMap, cv::COLORMAP_JET);

    cv::resize(colorMap,colorMap,cv::Size(sourceImage.width(),sourceImage.height()),0,0,cv::INTER_LINEAR);

    const QImage rgbImage = sourceImage.convertToFormat(QImage::Format_RGB888);

    cv::Mat SourceRgb(rgbImage.height(),rgbImage.width(),CV_8UC3,const_cast<uchar*>(rgbImage.constBits()),rgbImage.bytesPerLine());

    cv::Mat sourceBgr;
    cv::cvtColor(SourceRgb,sourceBgr,cv::COLOR_RGB2BGR);

    cv::Mat overlayBgr;
    cv::addWeighted(sourceBgr,1.0F-alpha,colorMap,alpha,0.0,overlayBgr);

    cv::Mat overlayRgb;
    cv::cvtColor(overlayBgr,overlayRgb,cv::COLOR_BGR2RGB);

    return QImage(overlayRgb.data,overlayRgb.cols,overlayRgb.rows,overlayRgb.step,QImage::Format_RGB888).copy();

}

int calulateDefectArea(const QImage& sourceImage, const bool* maskData, int maskWidth, int maskHeight)
{
    if(sourceImage.isNull() || maskData == nullptr)
    {
        return 0;
    }
    cv::Mat mask(maskHeight, maskWidth, CV_8UC1);

    for(int y = 0; y < maskHeight; ++y)
    {
        for(int x = 0; x < maskWidth; ++x)
        {
            mask.at<uchar>(y,x) = maskData[y * maskWidth + x] ? 255 : 0;
        }
    }
    cv::Mat originalSizeMask;
    cv::resize(mask,originalSizeMask,cv::Size(sourceImage.width(),sourceImage.height()),0,0,cv::INTER_NEAREST);
    return cv::countNonZero(originalSizeMask);
}

}//namespace





bool InferenceEngine::loadModel(const QString& modelPath, QString* errorMessage)
{
    if(errorMessage != nullptr)
    {
        errorMessage->clear();
    }

    const QFileInfo fileInfo(modelPath);

    if(!fileInfo.exists() || !fileInfo.isFile())
    {
        if(errorMessage != nullptr)
        {
            *errorMessage = "Model File Does Not Exist:\n" + modelPath;
        }
        return false;
    }

    const QString suffix = fileInfo.suffix().toLower();
    if(suffix != "xml" && suffix != "onnx")
    {
        if(errorMessage != nullptr)
        {
            *errorMessage = "Please Select an OpenVINO IR (*.xml) or ONNX (*.onnx) Model File.";
        }
        return false;
    }

    try
    {
        auto newModel = core.read_model(fileInfo.absoluteFilePath().toStdString());

        if(newModel->inputs().size() != 1)
        {
            throw std::runtime_error("Only single-input image models are supported.");
        }

        const ov::PartialShape inputShape = newModel->input().get_partial_shape();
        if(inputShape.rank().is_dynamic() || inputShape.rank().get_length() != 4)
        {
            throw std::runtime_error("Expected a four-dimensional NCHW input tensor.");
        }

        if(inputShape[1].is_dynamic() || inputShape[1].get_length() != 3)
        {
            throw std::runtime_error("Expected a three-channel NCHW input tensor.");
        }

        if(inputShape[2].is_dynamic() || inputShape[3].is_dynamic())
        {
            throw std::runtime_error("Dynamic image height or width is not supported.");
        }

        if(newModel->input().get_element_type() != ov::element::f32)
        {
            throw std::runtime_error("Expected a float32 input tensor.");
        }

        const int newInputHeight = static_cast<int>(inputShape[2].get_length());
        const int newInputWidth = static_cast<int>(inputShape[3].get_length());
        auto newCompiledModel = core.compile_model(newModel,"CPU");
        model = newModel;
        compiledModel = newCompiledModel;
        loadedModelPath = fileInfo.absoluteFilePath();
        inputHeight = newInputHeight;
        inputWidth = newInputWidth;
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


bool InferenceEngine::inferImage(const QImage& image, const InferenceOptions& options, InferenceResult* result, QString* errorMessage)
{
    if(errorMessage != nullptr)
    {
        errorMessage->clear();
    }

    if(result == nullptr)
    {
        if(errorMessage != nullptr)
        {
            *errorMessage = "InferenceResult Pointer is NUll";
        }
        return false;
    }
    *result = InferenceResult{};

    if(!isLoaded())
    {
        if(errorMessage != nullptr)
        {
            *errorMessage = "Please Load a Model Before Inference.";
        }
        return false;
    }

    if(image.isNull())
    {
        if(errorMessage != nullptr)
        {
            *errorMessage = "The Selected Image is Invalid.";
        }
        return false;
    }

    constexpr int channels = 3;

    QImage rgbImage = image.convertToFormat(QImage::Format_RGB888);

    rgbImage = rgbImage.scaled(inputWidth,inputHeight,Qt::IgnoreAspectRatio,Qt::SmoothTransformation);

    std::vector<float> inputData(channels * inputWidth * inputHeight);

    for(int y = 0; y < inputHeight; ++y)
    {
        const uchar* row = rgbImage.constScanLine(y);
        for(int x = 0; x < inputWidth; ++x)
        {
            const int rgbOffset = x * channels;
            const float red = row[rgbOffset] / 255.0F;
            const float green = row[rgbOffset+1] / 255.0F;
            const float blue = row[rgbOffset+2] / 255.0F;

            const int pixelIndex = y * inputWidth + x;

            inputData[0 * inputWidth * inputHeight + pixelIndex] = red;
            inputData[1 * inputWidth * inputHeight + pixelIndex] = green;
            inputData[2 * inputWidth * inputHeight + pixelIndex] = blue;
        }
    }
    try
    {
        ov::InferRequest inferRequest = compiledModel.create_infer_request();
        const ov::Shape inputShape = {
            1,
            static_cast<std::size_t>(channels),
            static_cast<std::size_t>(inputHeight),
            static_cast<std::size_t>(inputWidth)
        };
        ov::Tensor inputTensor(ov::element::f32,inputShape,inputData.data());
        inferRequest.set_input_tensor(inputTensor);
        QElapsedTimer timer;
        timer.start();

        inferRequest.infer();
        result->inferenceMs = static_cast<int>(timer.elapsed());

        const ov::Tensor scoreTensor = inferRequest.get_tensor("pred_score");

        //const ov::Tensor labelTensor = inferRequest.get_tensor("pred_label");
        result->score = scoreTensor.data<const float>()[0];

        //result->isNg = labelTensor.data<const bool>()[0];
        if (result->score < options.reviewThreshold) {
            result->detectionLevel = DetectionLevel::OK;
        }
        else if (result->score < options.ngThreshold) {
            result->detectionLevel = DetectionLevel::REVIEW;
        }
        else {
            result->detectionLevel = DetectionLevel::NG;
        }

        const ov::Tensor anomalyTensor = inferRequest.get_tensor("anomaly_map");
        const ov::Shape anomalyShape = anomalyTensor.get_shape();

        if(anomalyShape.size() != 4)
        {
            throw std::runtime_error("Unexpected Anomaly_map Tensor Shape.");
        }

        const int anomalyMapHeight = static_cast<int>(anomalyShape[2]);
        const int anomalyMapWidth = static_cast<int>(anomalyShape[3]);
        const float* anomalyData = anomalyTensor.data<const float>();
        const float heatmapAlpha = std::clamp(static_cast<float>(options.heatmapAlpha),0.0F,1.0F);
        result->heatmapOverlay = AssistFunc::createHeatmapOverlay(image,anomalyData,anomalyMapWidth,anomalyMapHeight,heatmapAlpha);

        const ov::Tensor maskTensor = inferRequest.get_tensor("pred_mask");
        const ov::Shape maskShape = maskTensor.get_shape();

        if(maskShape.size() !=4)
        {
            throw std::runtime_error(
                "Unexpected Pred_mask Tensor Shape."
                );
        }

        const int maskHeight = static_cast<int>(maskShape[2]);
        const int maskWidth = static_cast<int>(maskShape[3]);
        const bool* maskData = maskTensor.data<const bool>();

        result->defectArea = AssistFunc::calulateDefectArea(image,maskData,maskWidth,maskHeight);
        return true;
    }
    catch (const std::exception& error)
    {
        if(errorMessage != nullptr)
        {
            *errorMessage = "Inference failed:\n" + QString::fromLocal8Bit(error.what());
        }
        return false;
    }
}






















