#include "LatestCameraFrameBuffer.h"

std::optional<quint64> LatestCameraFrameBuffer::submit(DecodedCameraFrame frame)
{
    std::optional<quint64> replacedSequence;

    if(pendingFrame_.has_value())
    {
        replacedSequence = pendingFrame_->header.sequence;
        ++replacedFrameCount_;
    }

    pendingFrame_.emplace(std::move(frame));
    return replacedSequence;
}


std::optional<DecodedCameraFrame> LatestCameraFrameBuffer::take()
{
    if(!pendingFrame_.has_value())
    {
        return std::nullopt;
    }

    std::optional<DecodedCameraFrame> frame = std::move(pendingFrame_);

    pendingFrame_.reset();
    return frame;
}


bool LatestCameraFrameBuffer::hasPendingFrame() const noexcept
{
    return pendingFrame_.has_value();
}


quint64 LatestCameraFrameBuffer::replacedFrameCount() const noexcept
{
    return replacedFrameCount_;
}

void LatestCameraFrameBuffer::reset()
{
    pendingFrame_.reset();
    replacedFrameCount_ = 0;
}
