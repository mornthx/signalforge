// src/video/video_types.hpp
#pragma once

#include <QMetaType>
#include <cstdint>

namespace signalforge::video {

/// Periodic snapshot of the video receiver's throughput and health.
///
/// Emitted by `VideoUdpReceiver::statsUpdated` on a fixed cadence. `fps` and
/// `mbps` are rates over the most recent sampling window; the frame counters
/// are cumulative since the last bind.
struct VideoStats {
    double fps = 0.0;                   ///< Complete frames delivered per second (windowed).
    double mbps = 0.0;                  ///< Megabits/s of UDP payload received (windowed).
    std::uint64_t framesDelivered = 0;  ///< Cumulative complete frames delivered.
    std::uint64_t framesDropped = 0;    ///< Cumulative frames seen but never completed.
    int width = 0;                      ///< Width of the last delivered frame.
    int height = 0;                     ///< Height of the last delivered frame.
};

}  // namespace signalforge::video

Q_DECLARE_METATYPE(signalforge::video::VideoStats)
