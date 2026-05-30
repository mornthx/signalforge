// src/inspect/raw_frame_tap.hpp
#pragma once

#include "frame/raw_frame.hpp"
#include "pipeline/frame_sink.hpp"

#include <QByteArray>
#include <QString>
#include <chrono>
#include <cstdint>
#include <deque>
#include <mutex>
#include <vector>

namespace signalforge::inspect {

/// One captured frame, flattened from `RawFrame` for the Raw packet view.
struct CapturedFrame {
    std::uint64_t index = 0;  ///< Monotonic capture number (Wireshark "No.").
    QString source;           ///< Originating driver id.
    QString protocol;         ///< Protocol hint (serial/tcp/udp/replay).
    std::chrono::steady_clock::time_point recvAt{};
    std::uint64_t sequence = 0;  ///< Driver-local sequence number.
    QByteArray payload;          ///< Raw bytes (implicitly shared copy).
};

/// A `FrameSink` that ring-buffers recent raw frames from every pipeline it is
/// attached to — the data source for the Tier-1 Raw packet view (M31).
///
/// One tap is shared across all driver pipelines, so the view shows every
/// driver's packets together (the Wireshark model). Thread-safe: pipeline
/// threads append via `onFrame`; the UI thread reads via `snapshot`/`since`.
class RawFrameTap : public signalforge::pipeline::FrameSink {
public:
    explicit RawFrameTap(std::size_t capacity = 5000);
    ~RawFrameTap() override = default;

    RawFrameTap(const RawFrameTap&) = delete;
    RawFrameTap& operator=(const RawFrameTap&) = delete;

    // --- FrameSink (pipeline thread) ---
    void onFrame(const signalforge::frame::RawFrame& frame) override;
    [[nodiscard]] QString sinkName() const override;

    // --- UI thread ---
    /// All buffered frames, oldest first.
    [[nodiscard]] std::vector<CapturedFrame> snapshot() const;
    /// Buffered frames with `index` strictly greater than `afterIndex`
    /// (incremental polling). Oldest first.
    [[nodiscard]] std::vector<CapturedFrame> since(std::uint64_t afterIndex) const;
    /// Total frames ever captured (including evicted ones).
    [[nodiscard]] std::uint64_t totalCaptured() const;
    /// Drop all buffered frames (the capture counter keeps advancing).
    void clear();

private:
    mutable std::mutex mutex_;
    std::deque<CapturedFrame> frames_;
    std::size_t capacity_;
    std::uint64_t nextIndex_ = 1;
};

}  // namespace signalforge::inspect
