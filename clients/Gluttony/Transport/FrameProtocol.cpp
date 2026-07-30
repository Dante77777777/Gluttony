#include "FrameProtocol.h"

namespace FrameProtocolUtils {

void setError(QString* errorMessage, const QString& message)
{
    if(errorMessage != nullptr)
    {
        *errorMessage = message;
    }
}

bool isKnownMessageType(quint16 value)
{
    return value == static_cast<quint16>(FrameProtocol::MessageType::ImageFrame)
        || value == static_cast<quint16>(FrameProtocol::MessageType::Heartbeat)
        || value == static_cast<quint16>(FrameProtocol::MessageType::InferenceResult)
        || value == static_cast<quint16>(FrameProtocol::MessageType::InferenceError)
        || value == static_cast<quint16>(FrameProtocol::MessageType::StreamStart)
        || value == static_cast<quint16>(FrameProtocol::MessageType::StreamStop)
        || value == static_cast<quint16>(FrameProtocol::MessageType::StopAcquisitionRequest);
}

}

namespace FrameProtocol {

QByteArray encodePacket(FrameHeader header, const QByteArray& payload)
{
    if(static_cast<quint64>(payload.size()) > MAX_PAYLOAD_SIZE)
    {
        return {};
    }

    header.payloadSize = static_cast<quint32>(payload.size());
    QByteArray packet;
    packet.reserve(static_cast<qsizetype>(HEADER_SIZE) + payload.size());

    QDataStream stream(&packet,QIODevice::WriteOnly);

    stream.setByteOrder(QDataStream::BigEndian);
    stream.setVersion(QDataStream::Qt_6_5);

    stream
        << MAGIC
        << VERSION
        << HEADER_SIZE
        << static_cast<quint16>(header.messageType)
        << static_cast<quint16>(header.pixelFormat)
        << header.sequence
        << header.timestampMs
        << header.width
        << header.height
        << header.payloadSize;
    if(stream.status() != QDataStream::Ok || packet.size() != HEADER_SIZE)
    {
        return {};
    }

    packet.append(payload);
    return packet;
}


DecodeResult tryDecodePacket(QByteArray& receiveBuffer, FrameHeader& header, QByteArray& payload, QString* errorMessage)
{
    if(errorMessage != nullptr)
    {
        errorMessage->clear();
    }

    if(receiveBuffer.size() < HEADER_SIZE)
    {
        return DecodeResult::NeedMoreData;
    }

    const QByteArray headerBytes = receiveBuffer.left(HEADER_SIZE);
    QDataStream stream(headerBytes);

    stream.setByteOrder(QDataStream::BigEndian);
    stream.setVersion(QDataStream::Qt_6_5);

    quint32 magic = 0;
    quint16 version = 0;
    quint16 headerSize = 0;
    quint16 messageType = 0;
    quint16 pixelFormat = 0;
    quint64 sequence = 0;
    quint64 timestampMs = 0;
    quint32 width = 0;
    quint32 height = 0;
    quint32 payloadSize = 0;

    stream
        >> magic
        >> version
        >> headerSize
        >> messageType
        >> pixelFormat
        >> sequence
        >> timestampMs
        >> width
        >> height
        >> payloadSize;
    if(stream.status() != QDataStream::Ok)
    {
        FrameProtocolUtils::setError(errorMessage,QStringLiteral("Failed to read frame header"));
        return DecodeResult::InvalidData;
    }

    if(magic != MAGIC)
    {
        FrameProtocolUtils::setError(
            errorMessage,
            QStringLiteral("Invalid protocol magic")
            );
        return DecodeResult::InvalidData;
    }

    if(version != VERSION)
    {
        FrameProtocolUtils::setError(
            errorMessage,
            QStringLiteral("Unsupported protocol version")
            );
        return DecodeResult::InvalidData;
    }

    if(headerSize != HEADER_SIZE)
    {
        FrameProtocolUtils::setError(
            errorMessage,
            QStringLiteral("Invalid protocol header size")
            );
        return DecodeResult::InvalidData;
    }

    if(!FrameProtocolUtils::isKnownMessageType(messageType))
    {
        FrameProtocolUtils::setError(
            errorMessage,
            QStringLiteral("Unknown message type")
            );
        return DecodeResult::InvalidData;
    }

    if(payloadSize > MAX_PAYLOAD_SIZE)
    {
        FrameProtocolUtils::setError(
            errorMessage,
            QStringLiteral("Payload is too large")
            );
        return DecodeResult::InvalidData;
    }

    const qsizetype packetSize = static_cast<qsizetype>(HEADER_SIZE) + static_cast<qsizetype>(payloadSize);

    if(receiveBuffer.size() < packetSize)
    {
        return DecodeResult::NeedMoreData;
    }

    header.messageType = static_cast<MessageType>(messageType);
    header.pixelFormat = static_cast<PixelFormat>(pixelFormat);
    header.sequence = sequence;
    header.timestampMs = timestampMs;
    header.width = width;
    header.height = height;
    header.payloadSize = payloadSize;

    payload = receiveBuffer.mid(headerSize,payloadSize);

    receiveBuffer.remove(0,packetSize);
    return DecodeResult::Complete;
}
}
