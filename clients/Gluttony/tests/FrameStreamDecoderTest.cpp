#include <QSignalSpy>
#include <QtTest>

#include "Transport/FrameProtocol.h"
#include "Transport/FrameStreamDecoder.h"

class FrameStreamDecoderTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void fragmentedPacketEmitsAfterCompletion();
    void stickyPacketsEmitInOrder();
    void completePacketLeavesNextFragmentBuffered();
    void invalidPacketClearsBuffer();
    void resetClearsPartialPacket();
    void emptyInputDoesNothing();
};

namespace
{

QByteArray makePacket(
    quint64 sequence,
    const QByteArray& payload
)
{
    FrameProtocol::FrameHeader header;
    header.messageType =
        FrameProtocol::MessageType::ImageFrame;
    header.pixelFormat =
        FrameProtocol::PixelFormat::Jpeg;
    header.sequence = sequence;
    header.timestampMs = 1785211200123ULL + sequence;
    header.width = 640;
    header.height = 480;

    return FrameProtocol::encodePacket(
        header,
        payload
    );
}

ReceivedFramePacket frameAt(
    const QSignalSpy& spy,
    int index
)
{
    return qvariant_cast<ReceivedFramePacket>(
        spy.at(index).at(0)
    );
}

}

void FrameStreamDecoderTest::initTestCase()
{
    qRegisterMetaType<ReceivedFramePacket>(
        "ReceivedFramePacket"
    );
}

void FrameStreamDecoderTest::
    fragmentedPacketEmitsAfterCompletion()
{
    FrameStreamDecoder decoder;
    QSignalSpy frameSpy(
        &decoder,
        &FrameStreamDecoder::sigFrameDecoded
    );
    QSignalSpy errorSpy(
        &decoder,
        &FrameStreamDecoder::sigProtocolError
    );

    const QByteArray packet =
        makePacket(7, QByteArray("jpeg-payload"));

    decoder.appendBytes(packet.left(10));

    QCOMPARE(frameSpy.count(), 0);
    QCOMPARE(decoder.bufferedBytes(), 10);

    decoder.appendBytes(packet.mid(10, 25));

    QCOMPARE(frameSpy.count(), 0);
    QCOMPARE(decoder.bufferedBytes(), 35);

    decoder.appendBytes(packet.mid(35));

    QCOMPARE(frameSpy.count(), 1);
    QCOMPARE(errorSpy.count(), 0);
    QCOMPARE(decoder.bufferedBytes(), 0);

    const auto frame = frameAt(frameSpy, 0);

    QCOMPARE(frame.header.sequence, 7ULL);
    QCOMPARE(frame.header.width, 640U);
    QCOMPARE(frame.header.height, 480U);
    QCOMPARE(frame.payload, QByteArray("jpeg-payload"));
}

void FrameStreamDecoderTest::stickyPacketsEmitInOrder()
{
    FrameStreamDecoder decoder;
    QSignalSpy frameSpy(
        &decoder,
        &FrameStreamDecoder::sigFrameDecoded
    );

    const QByteArray firstPacket =
        makePacket(100, QByteArray("first"));
    const QByteArray secondPacket =
        makePacket(101, QByteArray("second"));

    decoder.appendBytes(
        firstPacket + secondPacket
    );

    QCOMPARE(frameSpy.count(), 2);
    QCOMPARE(decoder.bufferedBytes(), 0);

    const auto firstFrame = frameAt(frameSpy, 0);
    const auto secondFrame = frameAt(frameSpy, 1);

    QCOMPARE(firstFrame.header.sequence, 100ULL);
    QCOMPARE(firstFrame.payload, QByteArray("first"));
    QCOMPARE(secondFrame.header.sequence, 101ULL);
    QCOMPARE(secondFrame.payload, QByteArray("second"));
}

void FrameStreamDecoderTest::
    completePacketLeavesNextFragmentBuffered()
{
    FrameStreamDecoder decoder;
    QSignalSpy frameSpy(
        &decoder,
        &FrameStreamDecoder::sigFrameDecoded
    );

    const QByteArray firstPacket =
        makePacket(20, QByteArray("complete"));
    const QByteArray secondPacket =
        makePacket(21, QByteArray("fragmented"));
    constexpr qsizetype fragmentSize = 17;

    decoder.appendBytes(
        firstPacket
        + secondPacket.left(fragmentSize)
    );

    QCOMPARE(frameSpy.count(), 1);
    QCOMPARE(
        decoder.bufferedBytes(),
        fragmentSize
    );
    QCOMPARE(
        frameAt(frameSpy, 0).header.sequence,
        20ULL
    );

    decoder.appendBytes(
        secondPacket.mid(fragmentSize)
    );

    QCOMPARE(frameSpy.count(), 2);
    QCOMPARE(decoder.bufferedBytes(), 0);
    QCOMPARE(
        frameAt(frameSpy, 1).header.sequence,
        21ULL
    );
}

void FrameStreamDecoderTest::
    invalidPacketClearsBuffer()
{
    FrameStreamDecoder decoder;
    QSignalSpy frameSpy(
        &decoder,
        &FrameStreamDecoder::sigFrameDecoded
    );
    QSignalSpy errorSpy(
        &decoder,
        &FrameStreamDecoder::sigProtocolError
    );

    QByteArray packet =
        makePacket(1, QByteArray("payload"));
    packet[0] = '\0';

    decoder.appendBytes(packet);

    QCOMPARE(frameSpy.count(), 0);
    QCOMPARE(errorSpy.count(), 1);
    QCOMPARE(decoder.bufferedBytes(), 0);
    QCOMPARE(
        errorSpy.first().at(0).toString(),
        QStringLiteral("Invalid protocol magic")
    );

    decoder.appendBytes(
        makePacket(2, QByteArray("recovered"))
    );

    QCOMPARE(frameSpy.count(), 1);
    QCOMPARE(
        frameAt(frameSpy, 0).header.sequence,
        2ULL
    );
}

void FrameStreamDecoderTest::resetClearsPartialPacket()
{
    FrameStreamDecoder decoder;
    const QByteArray packet =
        makePacket(1, QByteArray("payload"));

    decoder.appendBytes(packet.left(12));

    QCOMPARE(decoder.bufferedBytes(), 12);

    decoder.reset();

    QCOMPARE(decoder.bufferedBytes(), 0);
}

void FrameStreamDecoderTest::emptyInputDoesNothing()
{
    FrameStreamDecoder decoder;
    QSignalSpy frameSpy(
        &decoder,
        &FrameStreamDecoder::sigFrameDecoded
    );
    QSignalSpy errorSpy(
        &decoder,
        &FrameStreamDecoder::sigProtocolError
    );
    QSignalSpy overflowSpy(
        &decoder,
        &FrameStreamDecoder::sigBufferOverflow
    );

    decoder.appendBytes(QByteArray());

    QCOMPARE(frameSpy.count(), 0);
    QCOMPARE(errorSpy.count(), 0);
    QCOMPARE(overflowSpy.count(), 0);
    QCOMPARE(decoder.bufferedBytes(), 0);
}

QTEST_APPLESS_MAIN(FrameStreamDecoderTest)

#include "FrameStreamDecoderTest.moc"
