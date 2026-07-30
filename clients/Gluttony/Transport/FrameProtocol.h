#ifndef FRAMEPROTOCOL_H
#define FRAMEPROTOCOL_H

#include <QByteArray>
#include <QString>
#include <QtGlobal>
#include <QDataStream>
#include <QIODevice>

namespace FrameProtocol {

// ASCII: LUST
inline constexpr quint32 MAGIC = 0x4C555354U;

inline constexpr quint16 VERSION = 1;

inline constexpr quint16 HEADER_SIZE = 40;

// 单帧最大 64 MB，防止异常长度导致内存耗尽
inline constexpr quint32 MAX_PAYLOAD_SIZE = 64U * 1024U * 1024U;

enum class MessageType : quint16
{
    ImageFrame = 1,
    Heartbeat = 2,
    InferenceResult = 3,
    InferenceError = 4
};

enum class PixelFormat : quint16
{
    Unknown = 0,
    Mono8 = 1,
    Bgr8 = 2,
    Rgb8 = 3,
    Jpeg = 100
};

struct FrameHeader
{
    MessageType messageType = MessageType::ImageFrame;
    PixelFormat pixelFormat = PixelFormat::Unknown;

    quint64 sequence = 0;
    quint64 timestampMs = 0;

    quint32 width = 0;
    quint32 height = 0;
    quint32 payloadSize = 0;
};

enum class DecodeResult
{
    Complete,
    NeedMoreData,
    InvalidData
};

QByteArray encodePacket(FrameHeader header, const QByteArray& payload);

DecodeResult tryDecodePacket(QByteArray& receiveBuffer, FrameHeader& header, QByteArray& payload, QString* errorMessage = nullptr);

}


#endif
