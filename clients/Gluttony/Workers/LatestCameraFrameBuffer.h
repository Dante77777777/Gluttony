#ifndef LATESTCAMERAFRAMEBUFFER_H
#define LATESTCAMERAFRAMEBUFFER_H

#include <optional>
#include <utility>

#include "../Transport/ReceivedFrameImageDecoder.h"


class LatestCameraFrameBuffer final
{
public:
    std::optional<quint64> submit(DecodedCameraFrame frame);

    std::optional<DecodedCameraFrame> take();

    bool hasPendingFrame() const noexcept;

    quint64 replacedFrameCount() const noexcept;

    void reset();

private:
    std::optional<DecodedCameraFrame> pendingFrame_;
    quint64 replacedFrameCount_ = 0;
};






















#endif
