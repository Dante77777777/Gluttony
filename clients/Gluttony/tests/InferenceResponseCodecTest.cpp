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


QTEST_APPLESS_MAIN(InferenceResponseCodecTest)

#include "InferenceResponseCodecTest.moc"
