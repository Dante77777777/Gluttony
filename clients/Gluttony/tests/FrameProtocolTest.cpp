#include <QtTest>

#include "Transport/FrameProtocol.h"

class FrameProtocolTest : public QObject
{
    Q_OBJECT

private slots:
    void decodesFixedLustPacket();
    void encodesFixedWireFormat();
    void fragmentedPacketWaitsForMoreBytes();
    void stickyPacketsDecodeInOrder();
    void inferenceMessagesRoundTrip();
    void controlMessagesRoundTrip();
    void invalidHeadersAreRejected();
};

namespace
{

const QByteArray& fixedWirePacket()
{
    static const QByteArray packet =
        QByteArray::fromHex(
            "4C555354"
            "0001"
            "0028"
            "0001"
            "0064"
            "0102030405060708"
            "1112131415161718"
            "00000280"
            "000001E0"
            "00000004"
            "FFD8FFD9"
        );

    return packet;
}

FrameProtocol::FrameHeader makeHeader(
    quint64 sequence
)
{
    FrameProtocol::FrameHeader header;
    header.messageType =
        FrameProtocol::MessageType::ImageFrame;
    header.pixelFormat =
        FrameProtocol::PixelFormat::Jpeg;
    header.sequence = sequence;
    header.timestampMs = 0x1112131415161718ULL;
    header.width = 640;
    header.height = 480;
    return header;
}

void setBigEndian16(
    QByteArray& bytes,
    qsizetype offset,
    quint16 value
)
{
    bytes[offset] =
        static_cast<char>((value >> 8) & 0xFFU);
    bytes[offset + 1] =
        static_cast<char>(value & 0xFFU);
}

void setBigEndian32(
    QByteArray& bytes,
    qsizetype offset,
    quint32 value
)
{
    bytes[offset] =
        static_cast<char>((value >> 24) & 0xFFU);
    bytes[offset + 1] =
        static_cast<char>((value >> 16) & 0xFFU);
    bytes[offset + 2] =
        static_cast<char>((value >> 8) & 0xFFU);
    bytes[offset + 3] =
        static_cast<char>(value & 0xFFU);
}

}

void FrameProtocolTest::decodesFixedLustPacket()
{
    QByteArray receiveBuffer = fixedWirePacket();
    FrameProtocol::FrameHeader header;
    QByteArray payload;
    QString errorMessage;

    const auto result =
        FrameProtocol::tryDecodePacket(
            receiveBuffer,
            header,
            payload,
            &errorMessage
        );

    QCOMPARE(
        static_cast<int>(result),
        static_cast<int>(
            FrameProtocol::DecodeResult::Complete
        )
    );
    QVERIFY(errorMessage.isEmpty());
    QVERIFY(receiveBuffer.isEmpty());
    QCOMPARE(
        static_cast<int>(header.messageType),
        static_cast<int>(
            FrameProtocol::MessageType::ImageFrame
        )
    );
    QCOMPARE(
        static_cast<int>(header.pixelFormat),
        static_cast<int>(
            FrameProtocol::PixelFormat::Jpeg
        )
    );
    QCOMPARE(header.sequence, 0x0102030405060708ULL);
    QCOMPARE(
        header.timestampMs,
        0x1112131415161718ULL
    );
    QCOMPARE(header.width, 640U);
    QCOMPARE(header.height, 480U);
    QCOMPARE(header.payloadSize, 4U);
    QCOMPARE(
        payload,
        QByteArray::fromHex("FFD8FFD9")
    );
}

void FrameProtocolTest::encodesFixedWireFormat()
{
    auto header = makeHeader(
        0x0102030405060708ULL
    );

    const QByteArray packet =
        FrameProtocol::encodePacket(
            header,
            QByteArray::fromHex("FFD8FFD9")
        );

    QCOMPARE(packet, fixedWirePacket());
}

void FrameProtocolTest::
    fragmentedPacketWaitsForMoreBytes()
{
    const QByteArray packet = fixedWirePacket();
    QByteArray receiveBuffer =
        packet.left(FrameProtocol::HEADER_SIZE + 2);
    const QByteArray originalFragment = receiveBuffer;

    FrameProtocol::FrameHeader header;
    QByteArray payload;

    auto result =
        FrameProtocol::tryDecodePacket(
            receiveBuffer,
            header,
            payload
        );

    QCOMPARE(
        static_cast<int>(result),
        static_cast<int>(
            FrameProtocol::DecodeResult::NeedMoreData
        )
    );
    QCOMPARE(receiveBuffer, originalFragment);
    QVERIFY(payload.isEmpty());

    receiveBuffer.append(
        packet.mid(FrameProtocol::HEADER_SIZE + 2)
    );

    result =
        FrameProtocol::tryDecodePacket(
            receiveBuffer,
            header,
            payload
        );

    QCOMPARE(
        static_cast<int>(result),
        static_cast<int>(
            FrameProtocol::DecodeResult::Complete
        )
    );
    QVERIFY(receiveBuffer.isEmpty());
    QCOMPARE(
        payload,
        QByteArray::fromHex("FFD8FFD9")
    );
}

void FrameProtocolTest::stickyPacketsDecodeInOrder()
{
    const QByteArray firstPacket =
        FrameProtocol::encodePacket(
            makeHeader(100),
            QByteArray("first")
        );
    const QByteArray secondPacket =
        FrameProtocol::encodePacket(
            makeHeader(101),
            QByteArray("second")
        );

    QByteArray receiveBuffer =
        firstPacket + secondPacket;

    FrameProtocol::FrameHeader header;
    QByteArray payload;

    auto result =
        FrameProtocol::tryDecodePacket(
            receiveBuffer,
            header,
            payload
        );

    QCOMPARE(
        static_cast<int>(result),
        static_cast<int>(
            FrameProtocol::DecodeResult::Complete
        )
    );
    QCOMPARE(header.sequence, 100ULL);
    QCOMPARE(payload, QByteArray("first"));
    QCOMPARE(receiveBuffer, secondPacket);

    result =
        FrameProtocol::tryDecodePacket(
            receiveBuffer,
            header,
            payload
        );

    QCOMPARE(
        static_cast<int>(result),
        static_cast<int>(
            FrameProtocol::DecodeResult::Complete
        )
    );
    QCOMPARE(header.sequence, 101ULL);
    QCOMPARE(payload, QByteArray("second"));
    QVERIFY(receiveBuffer.isEmpty());
}

void FrameProtocolTest::inferenceMessagesRoundTrip()
{
    const QList<FrameProtocol::MessageType> messageTypes{
        FrameProtocol::MessageType::InferenceResult,
        FrameProtocol::MessageType::InferenceError
    };

    for (const FrameProtocol::MessageType messageType
         : messageTypes)
    {
        auto expectedHeader = makeHeader(7001);
        expectedHeader.messageType = messageType;
        expectedHeader.pixelFormat =
            FrameProtocol::PixelFormat::Unknown;
        expectedHeader.width = 0;
        expectedHeader.height = 0;
        const QByteArray expectedPayload(
            "{\"status\":\"OK\"}");

        QByteArray receiveBuffer =
            FrameProtocol::encodePacket(
                expectedHeader,
                expectedPayload);
        QVERIFY(!receiveBuffer.isEmpty());

        FrameProtocol::FrameHeader actualHeader;
        QByteArray actualPayload;
        QString errorMessage;

        const auto result =
            FrameProtocol::tryDecodePacket(
                receiveBuffer,
                actualHeader,
                actualPayload,
                &errorMessage);

        QCOMPARE(
            static_cast<int>(result),
            static_cast<int>(
                FrameProtocol::DecodeResult::Complete));
        QVERIFY(errorMessage.isEmpty());
        QVERIFY(receiveBuffer.isEmpty());
        QCOMPARE(
            static_cast<int>(actualHeader.messageType),
            static_cast<int>(messageType));
        QCOMPARE(actualHeader.sequence, 7001ULL);
        QCOMPARE(
            static_cast<int>(actualHeader.pixelFormat),
            static_cast<int>(
                FrameProtocol::PixelFormat::Unknown));
        QCOMPARE(actualHeader.width, 0U);
        QCOMPARE(actualHeader.height, 0U);
        QCOMPARE(actualPayload, expectedPayload);
    }
}

void FrameProtocolTest::controlMessagesRoundTrip()
{
    const QList<FrameProtocol::MessageType> messageTypes{
        FrameProtocol::MessageType::StreamStart,
        FrameProtocol::MessageType::StreamStop,
        FrameProtocol::MessageType::StopAcquisitionRequest
    };

    for(const auto messageType : messageTypes)
    {
        FrameProtocol::FrameHeader expectedHeader;
        expectedHeader.messageType = messageType;
        expectedHeader.pixelFormat =
            FrameProtocol::PixelFormat::Unknown;
        expectedHeader.sequence = 8100
            + static_cast<quint16>(messageType);
        expectedHeader.timestampMs = 1'785'211'200'000ULL;

        QByteArray receiveBuffer =
            FrameProtocol::encodePacket(expectedHeader, {});
        FrameProtocol::FrameHeader actualHeader;
        QByteArray actualPayload;
        QString errorMessage;

        QCOMPARE(
            FrameProtocol::tryDecodePacket(
                receiveBuffer,
                actualHeader,
                actualPayload,
                &errorMessage),
            FrameProtocol::DecodeResult::Complete);
        QVERIFY(errorMessage.isEmpty());
        QCOMPARE(actualHeader.messageType, messageType);
        QCOMPARE(
            actualHeader.sequence,
            expectedHeader.sequence);
        QVERIFY(actualPayload.isEmpty());
        QVERIFY(receiveBuffer.isEmpty());
    }
}

void FrameProtocolTest::invalidHeadersAreRejected()
{
    FrameProtocol::FrameHeader header;
    QByteArray payload;
    QString errorMessage;

    QByteArray invalidMagic = fixedWirePacket();
    invalidMagic[0] = '\0';

    QCOMPARE(
        static_cast<int>(
            FrameProtocol::tryDecodePacket(
                invalidMagic,
                header,
                payload,
                &errorMessage
            )
        ),
        static_cast<int>(
            FrameProtocol::DecodeResult::InvalidData
        )
    );
    QCOMPARE(
        errorMessage,
        QStringLiteral("Invalid protocol magic")
    );

    QByteArray invalidVersion = fixedWirePacket();
    setBigEndian16(invalidVersion, 4, 2);

    QCOMPARE(
        static_cast<int>(
            FrameProtocol::tryDecodePacket(
                invalidVersion,
                header,
                payload,
                &errorMessage
            )
        ),
        static_cast<int>(
            FrameProtocol::DecodeResult::InvalidData
        )
    );
    QCOMPARE(
        errorMessage,
        QStringLiteral("Unsupported protocol version")
    );

    QByteArray oversizedPayload = fixedWirePacket();
    setBigEndian32(
        oversizedPayload,
        36,
        FrameProtocol::MAX_PAYLOAD_SIZE + 1
    );

    QCOMPARE(
        static_cast<int>(
            FrameProtocol::tryDecodePacket(
                oversizedPayload,
                header,
                payload,
                &errorMessage
            )
        ),
        static_cast<int>(
            FrameProtocol::DecodeResult::InvalidData
        )
    );
    QCOMPARE(
        errorMessage,
        QStringLiteral("Payload is too large")
    );
}

QTEST_APPLESS_MAIN(FrameProtocolTest)

#include "FrameProtocolTest.moc"
