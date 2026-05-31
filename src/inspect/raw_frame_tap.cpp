// src/inspect/raw_frame_tap.cpp

#include "inspect/raw_frame_tap.hpp"

namespace signalforge::inspect {

RawFrameTap::RawFrameTap(std::size_t capacity) : capacity_(capacity == 0 ? 1 : capacity) {}

void RawFrameTap::onFrame(const signalforge::frame::RawFrame& frame) {
    const std::lock_guard<std::mutex> lock(mutex_);
    CapturedFrame captured;
    captured.index = nextIndex_++;
    captured.source = frame.sourceId;
    captured.protocol = frame.protocolHint;
    captured.recvAt = frame.recvAt;
    captured.sequence = frame.sequenceNumber;
    captured.payload = frame.payload;
    frames_.push_back(std::move(captured));
    while (frames_.size() > capacity_) {
        frames_.pop_front();
    }
}

QString RawFrameTap::sinkName() const {
    return QStringLiteral("raw-frame-tap");
}

std::vector<CapturedFrame> RawFrameTap::snapshot() const {
    const std::lock_guard<std::mutex> lock(mutex_);
    return {frames_.begin(), frames_.end()};
}

std::vector<CapturedFrame> RawFrameTap::since(std::uint64_t afterIndex) const {
    const std::lock_guard<std::mutex> lock(mutex_);
    std::vector<CapturedFrame> out;
    for (const CapturedFrame& f : frames_) {
        if (f.index > afterIndex) {
            out.push_back(f);
        }
    }
    return out;
}

std::uint64_t RawFrameTap::totalCaptured() const {
    const std::lock_guard<std::mutex> lock(mutex_);
    return nextIndex_ - 1;
}

void RawFrameTap::clear() {
    const std::lock_guard<std::mutex> lock(mutex_);
    frames_.clear();
}

}  // namespace signalforge::inspect
