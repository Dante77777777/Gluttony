#include "FrameStreamDecoder.h"

FrameStreamDecoder::FrameStreamDecoder(QObject* parent)
    : QObject(parent)
{
}

qsizetype FrameStreamDecoder::bufferedBytes() const noexcept
{
    return receiveBuffer_.size();
}

void FrameStreamDecoder::reset()
{
    receiveBuffer_.clear();
}

void FrameStreamDecoder::appendBytes(const QByteArray& bytes)
{
    if(bytes.isEmpty())
    {
        return ;
    }

    receiveBuffer_.append(bytes);

    while (true)
    {
        FrameProtocol::FrameHeader header;
        QByteArray payload;
        QString errorMessage;

        const auto result = FrameProtocol::tryDecodePacket(receiveBuffer_,header,payload,&errorMessage);
        if(result == FrameProtocol::DecodeResult::Complete)
        {
            ReceivedFramePacket frame;
            frame.header = header;
            frame.payload = std::move(payload);
            emit sigFrameDecoded(frame);
            /*
             * receiveBuffer_ 中可能还有下一个粘连包，
             * 因此继续解析。
             */
            continue;
        }

        if(result == FrameProtocol::DecodeResult::InvalidData)
        {
            receiveBuffer_.clear();
            if(errorMessage.isEmpty())
            {
                errorMessage = QStringLiteral("Invalid Frame Protocol Data");
            }
            emit sigProtocolError(errorMessage);
            return ;
        }

        /*
         * NeedMoreData：
         * 当前数据还不足以组成完整包。
         */
        if(receiveBuffer_.size() > MAX_BUFFERED_BYTES)
        {
            const qsizetype bufferedSize = receiveBuffer_.size();
            receiveBuffer_.clear();
            emit sigBufferOverflow(bufferedSize);
            return ;
        }
        return ;
    }
}
