#include <QtTest>

#include <limits>

#include "Transport/InferenceResponseCodec.h"

class InferenceResponseCodecTest : public QObject
{
    Q_OBJECT

private slots:
    void resultRoundTripPreservesFields();
    void encoderRejectsInvalidResultFields();
    void decoderRejectsMalformedJson();
    void decoderRejectsUnsupportedSchema();
    void decoderRejectsInvalidResultFields();
    void errorRoundTripPreservesFields();
    void encoderRejectsInvalidErrorFields();
    void decoderRejectsMalformedErrorJsonAndSchema();
    void decoderRejectsInvalidErrorFields();
};


void InferenceResponseCodecTest::
    resultRoundTripPreservesFields()
{
    InferenceResponseCodec::InferenceResultPayload expected;
    expected.detectionLevel = QStringLiteral("REVIEW");
    expected.score = 0.573;
    expected.inferenceMs = 18;
    expected.defectArea = 126;

    QString errorMessage;
    const QByteArray jsonBytes =
        InferenceResponseCodec::encodeResult(
            expected,
            &errorMessage);

    QVERIFY2(
        !jsonBytes.isEmpty(),
        qPrintable(errorMessage));
    QVERIFY(errorMessage.isEmpty());

    InferenceResponseCodec::InferenceResultPayload actual;
    QVERIFY2(
        InferenceResponseCodec::decodeResult(
            jsonBytes,
            &actual,
            &errorMessage),
        qPrintable(errorMessage));
    QVERIFY(errorMessage.isEmpty());
    QCOMPARE(
        actual.detectionLevel,
        expected.detectionLevel);
    QCOMPARE(actual.score, expected.score);
    QCOMPARE(actual.inferenceMs, expected.inferenceMs);
    QCOMPARE(actual.defectArea, expected.defectArea);
}


void InferenceResponseCodecTest::
    encoderRejectsInvalidResultFields()
{
    InferenceResponseCodec::InferenceResultPayload payload;
    payload.detectionLevel = QStringLiteral("UNKNOWN");

    QString errorMessage;
    QVERIFY(
        InferenceResponseCodec::encodeResult(
            payload,
            &errorMessage)
            .isEmpty());
    QVERIFY(!errorMessage.isEmpty());

    payload.detectionLevel = QStringLiteral("OK");
    payload.score =
        std::numeric_limits<double>::quiet_NaN();
    QVERIFY(
        InferenceResponseCodec::encodeResult(
            payload,
            &errorMessage)
            .isEmpty());
    QVERIFY(!errorMessage.isEmpty());

    payload.score = 0.2;
    payload.inferenceMs = -1;
    QVERIFY(
        InferenceResponseCodec::encodeResult(
            payload,
            &errorMessage)
            .isEmpty());
    QVERIFY(!errorMessage.isEmpty());

    payload.inferenceMs = 1;
    payload.defectArea = -1;
    QVERIFY(
        InferenceResponseCodec::encodeResult(
            payload,
            &errorMessage)
            .isEmpty());
    QVERIFY(!errorMessage.isEmpty());
}


void InferenceResponseCodecTest::
    decoderRejectsMalformedJson()
{
    InferenceResponseCodec::InferenceResultPayload result;
    QString errorMessage;

    QVERIFY(
        !InferenceResponseCodec::decodeResult(
            QByteArray("{not-json"),
            &result,
            &errorMessage));
    QVERIFY(!errorMessage.isEmpty());
}


void InferenceResponseCodecTest::
    decoderRejectsUnsupportedSchema()
{
    const QByteArray jsonBytes(
        R"({"schema_version":2,"detection_level":"OK","score":0.1,"inference_ms":2,"defect_area":0})");
    InferenceResponseCodec::InferenceResultPayload result;
    QString errorMessage;

    QVERIFY(
        !InferenceResponseCodec::decodeResult(
            jsonBytes,
            &result,
            &errorMessage));
    QVERIFY(!errorMessage.isEmpty());
}


void InferenceResponseCodecTest::
    decoderRejectsInvalidResultFields()
{
    const QList<QByteArray> invalidPayloads{
        QByteArray(
            R"({"schema_version":1,"detection_level":"UNKNOWN","score":0.1,"inference_ms":2,"defect_area":0})"),
        QByteArray(
            R"({"schema_version":1,"detection_level":"OK","score":"bad","inference_ms":2,"defect_area":0})"),
        QByteArray(
            R"({"schema_version":1,"detection_level":"OK","score":0.1,"inference_ms":-1,"defect_area":0})"),
        QByteArray(
            R"({"schema_version":1,"detection_level":"OK","score":0.1,"inference_ms":2,"defect_area":1.5})")
    };

    for (const QByteArray& jsonBytes : invalidPayloads)
    {
        InferenceResponseCodec::InferenceResultPayload result;
        QString errorMessage;

        QVERIFY(
            !InferenceResponseCodec::decodeResult(
                jsonBytes,
                &result,
                &errorMessage));
        QVERIFY(!errorMessage.isEmpty());
    }
}

void InferenceResponseCodecTest::
    errorRoundTripPreservesFields()
{
    InferenceResponseCodec::InferenceErrorPayload expected;
    expected.errorCode =
        QStringLiteral("INFERENCE_FAILED");
    expected.message =
        QStringLiteral("OpenVINO request failed");

    QString errorMessage;
    const QByteArray jsonBytes =
        InferenceResponseCodec::encodeError(
            expected,
            &errorMessage);

    QVERIFY2(
        !jsonBytes.isEmpty(),
        qPrintable(errorMessage));
    QVERIFY(errorMessage.isEmpty());

    InferenceResponseCodec::InferenceErrorPayload actual;
    QVERIFY2(
        InferenceResponseCodec::decodeError(
            jsonBytes,
            &actual,
            &errorMessage),
        qPrintable(errorMessage));
    QVERIFY(errorMessage.isEmpty());
    QCOMPARE(actual.errorCode, expected.errorCode);
    QCOMPARE(actual.message, expected.message);
}


void InferenceResponseCodecTest::
    encoderRejectsInvalidErrorFields()
{
    InferenceResponseCodec::InferenceErrorPayload payload;
    payload.message = QStringLiteral("message");

    QString errorMessage;
    QVERIFY(
        InferenceResponseCodec::encodeError(
            payload,
            &errorMessage)
            .isEmpty());
    QVERIFY(!errorMessage.isEmpty());

    payload.errorCode =
        QStringLiteral("INFERENCE_FAILED");
    payload.message.clear();
    QVERIFY(
        InferenceResponseCodec::encodeError(
            payload,
            &errorMessage)
            .isEmpty());
    QVERIFY(!errorMessage.isEmpty());

    payload.errorCode = QString(
        InferenceResponseCodec::
            MAX_ERROR_CODE_LENGTH + 1,
        QLatin1Char('A'));
    payload.message = QStringLiteral("message");
    QVERIFY(
        InferenceResponseCodec::encodeError(
            payload,
            &errorMessage)
            .isEmpty());
    QVERIFY(!errorMessage.isEmpty());

    payload.errorCode = QStringLiteral("FAILED");
    payload.message = QString(
        InferenceResponseCodec::
            MAX_ERROR_MESSAGE_LENGTH + 1,
        QLatin1Char('B'));
    QVERIFY(
        InferenceResponseCodec::encodeError(
            payload,
            &errorMessage)
            .isEmpty());
    QVERIFY(!errorMessage.isEmpty());
}


void InferenceResponseCodecTest::
    decoderRejectsMalformedErrorJsonAndSchema()
{
    InferenceResponseCodec::InferenceErrorPayload result;
    QString errorMessage;

    QVERIFY(
        !InferenceResponseCodec::decodeError(
            QByteArray("{not-json"),
            &result,
            &errorMessage));
    QVERIFY(!errorMessage.isEmpty());

    const QByteArray unsupportedSchema(
        R"({"schema_version":2,"error_code":"FAILED","message":"message"})");
    QVERIFY(
        !InferenceResponseCodec::decodeError(
            unsupportedSchema,
            &result,
            &errorMessage));
    QVERIFY(!errorMessage.isEmpty());
}


void InferenceResponseCodecTest::
    decoderRejectsInvalidErrorFields()
{
    const QList<QByteArray> invalidPayloads{
        QByteArray(
            R"({"schema_version":1,"error_code":"","message":"message"})"),
        QByteArray(
            R"({"schema_version":1,"error_code":"FAILED","message":""})"),
        QByteArray(
            R"({"schema_version":1,"error_code":42,"message":"message"})"),
        QByteArray(
            R"({"schema_version":1,"error_code":"FAILED","message":42})")
    };

    for (const QByteArray& jsonBytes : invalidPayloads)
    {
        InferenceResponseCodec::InferenceErrorPayload result;
        QString errorMessage;

        QVERIFY(
            !InferenceResponseCodec::decodeError(
                jsonBytes,
                &result,
                &errorMessage));
        QVERIFY(!errorMessage.isEmpty());
    }
}


QTEST_APPLESS_MAIN(InferenceResponseCodecTest)

#include "InferenceResponseCodecTest.moc"
