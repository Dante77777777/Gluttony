#ifndef INFERENCERESPONSECODEC_H
#define INFERENCERESPONSECODEC_H

#include <QByteArray>
#include <QString>


namespace InferenceResponseCodec {

inline constexpr int SCHEMA_VERSION = 1;
inline constexpr int MAX_ERROR_CODE_LENGTH = 64;
inline constexpr int MAX_ERROR_MESSAGE_LENGTH = 1024;

struct InferenceResultPayload
{
    QString detectionLevel;
    double score = 0.0;
    int inferenceMs = 0;
    int defectArea = 0;
};

struct InferenceErrorPayload
{
    QString errorCode;
    QString message;
};

QByteArray encodeResult(const InferenceResultPayload& payload, QString* errorMessage = nullptr);

bool decodeResult(const QByteArray& jsonBytes, InferenceResultPayload* result, QString* errorMessage = nullptr);

QByteArray encodeError(const InferenceErrorPayload& payload, QString* errorMessage = nullptr);

bool decodeError(const QByteArray& jsonBytes, InferenceErrorPayload* result, QString* errorMessage = nullptr);


}

#endif
