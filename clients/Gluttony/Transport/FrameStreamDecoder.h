#ifndef FRAMESTREAMDECODER_H
#define FRAMESTREAMDECODER_H

#include <QByteArray>
#include <QMetaType>
#include <QObject>
#include <QString>

#include <utility>

#include "FrameProtocol.h"

struct ReceivedFramePacket
{
    FrameProtocol::FrameHeader header;
    QByteArray payload;
};

Q_DECLARE_METATYPE(ReceivedFramePacket)

class FrameStreamDecoder final : public QObject
{
    Q_OBJECT

public:
    static constexpr qsizetype MAX_BUFFERED_BYTES = static_cast<qsizetype>(FrameProtocol::HEADER_SIZE + static_cast<qsizetype>(FrameProtocol::MAX_PAYLOAD_SIZE));

    explicit FrameStreamDecoder(QObject* parent = nullptr);

    qsizetype bufferedBytes() const noexcept;

    void reset();

public slots:
    void appendBytes(const QByteArray& bytes);

signals:
    void sigFrameDecoded(const ReceivedFramePacket& frame);

    void sigProtocolError(const QString& errorMessage);

    void sigBufferOverflow(qsizetype bufferBytes);

private:
    QByteArray receiveBuffer_;
};

#endif
