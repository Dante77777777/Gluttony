#ifndef RECEIVEDFRAMEIMAGEDECODER_H
#define RECEIVEDFRAMEIMAGEDECODER_H

#include <QImage>
#include <QString>
#include <QtGlobal>
#include <QBuffer>
#include <QImageReader>
#include <QIODevice>
#include <QSize>
#include <QMetaType>

#include "FrameStreamDecoder.h"

struct DecodedCameraFrame
{
    FrameProtocol::FrameHeader header;
    QImage image;
};
Q_DECLARE_METATYPE(DecodedCameraFrame)

class ReceivedFrameImageDecoder final
{
public:
    static constexpr quint64 MAX_IMAGE_PIXELS = 100ULL * 1000ULL * 1000ULL;
    static bool decode(const ReceivedFramePacket& packet, DecodedCameraFrame* result, QString* errorMessage = nullptr);
};


#endif
