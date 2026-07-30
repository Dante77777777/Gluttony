#ifndef INFERENCERESPONSECODEC_H
#define INFERENCERESPONSECODEC_H

#include <QByteArray>
#include <QString>


namespace InferenceResponseCodec {

inline constexpr int SCHEMA_VERSION = 1;
struct InferenceResultPayload
{
    QString detectionLevel;
    double score = 0.0;
    int inferenceMs = 0;
    int defectArea = 0;
};

QByteArray encodeResult(const InferenceResultPayload& payload, QString* errorMessage = nullptr);

bool decodeResult(const QByteArray& jsonBytes, InferenceResultPayload* result, QString* errorMessage = nullptr);
}

#endif
