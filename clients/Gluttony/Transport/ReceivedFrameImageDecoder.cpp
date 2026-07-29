#include "ReceivedFrameImageDecoder.h"

namespace Utils {

void setDecodeError(QString* errorMessage, const QString& message)
{
    if(errorMessage != nullptr)
    {
        *errorMessage = message;
    }
}
}

bool ReceivedFrameImageDecoder::decode(const ReceivedFramePacket& packet, DecodedCameraFrame* result, QString* errorMessage)
{
    if(errorMessage != nullptr)
    {
        errorMessage->clear();
    }

    if(result == nullptr)
    {
        Utils::setDecodeError(errorMessage,QStringLiteral("Decoded frame result pointer is null"));
        return false;
    }

    *result = DecodedCameraFrame{};

    if(packet.header.messageType != FrameProtocol::MessageType::ImageFrame)
    {
        Utils::setDecodeError(errorMessage,QStringLiteral("The received packet is not an image frame"));
        return false;
    }

    if(packet.header.pixelFormat != FrameProtocol::PixelFormat::Jpeg)
    {
        Utils::setDecodeError(errorMessage,QStringLiteral("Unsupported camera frame pixel format"));
        return false;
    }

    if(packet.payload.isEmpty())
    {
        Utils::setDecodeError(errorMessage,QStringLiteral("The received image payload is empty"));
        return false;
    }

    if(packet.header.width == 0 || packet.header.height == 0)
    {
        Utils::setDecodeError(errorMessage,QStringLiteral("The received image dimensions are invalid"));
        return false;
    }

    const quint64 pixelCount = static_cast<quint64>(packet.header.width) * static_cast<quint64>(packet.header.height);

    if(pixelCount > MAX_IMAGE_PIXELS)
    {
        Utils::setDecodeError(errorMessage,QStringLiteral("The received image dimensions are too large"));
        return false;
    }

    QBuffer payloadBuffer;
    payloadBuffer.setData(packet.payload);

    if(!payloadBuffer.open(QIODevice::ReadOnly))
    {
        Utils::setDecodeError(errorMessage,QStringLiteral("Failed to open the image payload buffer"));
        return false;
    }

    QImageReader reader(&payloadBuffer,"JPEG");

    const QSize encodedSize = reader.size();

    if(!encodedSize.isValid())
    {
        Utils::setDecodeError(errorMessage,QStringLiteral("Failed to read the JPEG dimensions: %1").arg(reader.errorString()));
        return false;
    }

    if(encodedSize.width() != static_cast<int>(packet.header.width) || encodedSize.height() != static_cast<int>(packet.header.height))
    {
        Utils::setDecodeError(errorMessage,QStringLiteral("JPEG dimensions do not match the protocol header"));
        return false;
    }

    QImage decodedImage = reader.read();

    if(decodedImage.isNull())
    {
        Utils::setDecodeError(errorMessage,QStringLiteral("Failed to decode the JPEG image: %1").arg(reader.errorString()));
        return false;
    }

    result->header = packet.header;
    result->image = decodedImage.convertToFormat(QImage::Format_RGB888);
    return true;
}
