#include "InferenceResponseCodec.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>

#include <cmath>
#include <limits>

namespace
{
void setError(QString* errorMessage, const QString& message)
{
    if(errorMessage != nullptr)
    {
        *errorMessage = message;
    }
}

bool isValidDetectionLevel(const QString& detectionLevel)
{
    return detectionLevel == QStringLiteral("OK")
        || detectionLevel == QStringLiteral("REVIEW")
        || detectionLevel == QStringLiteral("NG");
}

bool readNonNegativeInteger(const QJsonValue& value, int* result)
{
    if(!result || !value.isDouble())
    {
        return false;
    }

    const double number = value.toDouble(-1.0);

    if(!std::isfinite(number) || number < 0.0 || number > static_cast<double>(std::numeric_limits<int>::max()) || std::floor(number) != number)
    {
        return false;
    }
    *result = static_cast<int>(number);
    return true;
}

}


namespace InferenceResponseCodec
{

QByteArray encodeResult(const InferenceResultPayload& payload, QString* errorMessage)
{
    if(errorMessage)
    {
        errorMessage->clear();
    }

    if(!isValidDetectionLevel(payload.detectionLevel))
    {
        setError(errorMessage,QStringLiteral("Invalid Detection Level"));
        return {};
    }

    if(!std::isfinite(payload.score))
    {
        setError(errorMessage, QStringLiteral("Inference score must be finite"));
        return {};
    }

    if(payload.inferenceMs < 0)
    {
        setError(errorMessage, QStringLiteral("Inference time cannot be negative"));
        return {};
    }

    if(payload.defectArea < 0)
    {
        setError(errorMessage, QStringLiteral("Defect area cannot be negative"));
        return {};
    }

    QJsonObject object;

    object.insert(QStringLiteral("schema_version"), SCHEMA_VERSION);
    object.insert(QStringLiteral("detection_level"), payload.detectionLevel);
    object.insert(QStringLiteral("score"), payload.score);
    object.insert(QStringLiteral("inference_ms"), payload.inferenceMs);
    object.insert(QStringLiteral("defect_area"), payload.defectArea);

    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}


bool decodeResult(const QByteArray& jsonBytes, InferenceResultPayload* result, QString* errorMessage)
{
    if(errorMessage)
    {
        errorMessage->clear();
    }

    if(!result)
    {
        setError(errorMessage,QStringLiteral("Result payload pointer is null"));
        return false;
    }

    *result = InferenceResultPayload{};

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(jsonBytes,&parseError);

    if(parseError.error != QJsonParseError::NoError || !document.isObject())
    {
        setError(errorMessage,QStringLiteral("Invalid inference result JSON"));
        return false;
    }

    const QJsonObject object = document.object();
    if(object.value(QStringLiteral("schema_version")).toInt(-1) != SCHEMA_VERSION)
    {
        setError(errorMessage,QStringLiteral("Unsupported inference result schema"));
        return false;
    }

    const QJsonValue detectionLevelValue =
        object.value(
            QStringLiteral("detection_level"));

    if(!detectionLevelValue.isString() || !isValidDetectionLevel(detectionLevelValue.toString()))
    {
        setError(errorMessage, QStringLiteral("Invalid detection level"));
        return false;
    }

    const QJsonValue scoreValue = object.value(QStringLiteral("score"));

    if(!scoreValue.isDouble() || !std::isfinite(scoreValue.toDouble()))
    {
        setError(errorMessage, QStringLiteral("Invalid inference score"));
        return false;
    }

    int inferenceMs = 0;
    if(!readNonNegativeInteger(object.value(QStringLiteral("inference_ms")),&inferenceMs))
    {
        setError(errorMessage,QStringLiteral("Invalid inference time"));
        return false;
    }

    int defectArea = 0;
    if(!readNonNegativeInteger(object.value(QStringLiteral("defect_area")),&defectArea))
    {
        setError(errorMessage, QStringLiteral("Invalid defect area"));
        return false;
    }

    result->detectionLevel = detectionLevelValue.toString();
    result->score = scoreValue.toDouble();
    result->inferenceMs = inferenceMs;
    result->defectArea = defectArea;
    return true;
}




















}
