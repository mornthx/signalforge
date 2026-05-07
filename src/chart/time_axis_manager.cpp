// src/chart/time_axis_manager.cpp
//
// State machine: visible range = [visibleEnd - duration, visibleEnd].
// In live mode, `visibleEnd()` returns `steady_clock::now()` on every
// call; in paused mode it returns the captured `pausedAt_`. Pan / zoom
// while live transition to paused (anchoring the current visibleEnd
// before applying the operation). `rangeChanged` fires on every state
// change; `liveModeChanged(bool)` fires on live ↔ paused transitions.

#include "chart/time_axis_manager.hpp"

namespace signalforge::chart {

namespace {

constexpr auto kDefaultDuration = std::chrono::seconds(10);

[[nodiscard]] TimeAxisManager::Duration durationFromPreset(TimeAxisManager::TimePreset p) noexcept {
    using std::chrono::duration_cast;
    using std::chrono::nanoseconds;
    using std::chrono::seconds;
    switch (p) {
    case TimeAxisManager::TimePreset::Sec1:
        return duration_cast<nanoseconds>(seconds(1));
    case TimeAxisManager::TimePreset::Sec10:
        return duration_cast<nanoseconds>(seconds(10));
    case TimeAxisManager::TimePreset::Min1:
        return duration_cast<nanoseconds>(seconds(60));
    case TimeAxisManager::TimePreset::Min10:
        return duration_cast<nanoseconds>(seconds(600));
    case TimeAxisManager::TimePreset::Hour1:
        return duration_cast<nanoseconds>(seconds(3600));
    }
    return duration_cast<nanoseconds>(kDefaultDuration);
}

}  // namespace

TimeAxisManager::TimeAxisManager(QObject* parent)
    : QObject(parent), duration_(std::chrono::duration_cast<Duration>(kDefaultDuration)) {}

TimeAxisManager::~TimeAxisManager() = default;

TimeAxisManager::TimePoint TimeAxisManager::visibleStart() const noexcept {
    return visibleEnd() - duration_;
}

TimeAxisManager::TimePoint TimeAxisManager::visibleEnd() const noexcept {
    return liveMode_ ? std::chrono::steady_clock::now() : pausedAt_;
}

TimeAxisManager::Duration TimeAxisManager::visibleDuration() const noexcept {
    return duration_;
}

bool TimeAxisManager::liveMode() const noexcept {
    return liveMode_;
}

void TimeAxisManager::pan(Duration offset) {
    const bool wasLive = liveMode_;
    if (wasLive) {
        // Anchor at the current visibleEnd before transitioning, so
        // the user's panning frame of reference is "where the chart
        // is right now", not "now() at the moment of the next read".
        pausedAt_ = std::chrono::steady_clock::now();
        liveMode_ = false;
    }
    pausedAt_ += offset;
    if (wasLive) {
        Q_EMIT liveModeChanged(false);
    }
    Q_EMIT rangeChanged();
}

void TimeAxisManager::zoom(double factor, TimePoint referencePoint) {
    if (factor <= 0.0) {
        return;  // Defensive: zoom factor must be positive
    }
    const bool wasLive = liveMode_;
    const TimePoint anchor = wasLive ? std::chrono::steady_clock::now() : pausedAt_;

    // The reference point's distance to anchor scales by `factor`;
    // the duration also scales by `factor`. The reference point's
    // logical position relative to the visible window stays the
    // same.
    const auto refOffsetNs =
        static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(anchor - referencePoint).count());
    const auto newRefOffsetNs = static_cast<long long>(refOffsetNs * factor);
    const auto newDurationNs = static_cast<long long>(static_cast<double>(duration_.count()) * factor);

    duration_ = Duration(newDurationNs);
    pausedAt_ = referencePoint + Duration(newRefOffsetNs);
    if (wasLive) {
        liveMode_ = false;
    }
    if (wasLive) {
        Q_EMIT liveModeChanged(false);
    }
    Q_EMIT rangeChanged();
}

void TimeAxisManager::pause() {
    if (!liveMode_) {
        return;
    }
    pausedAt_ = std::chrono::steady_clock::now();
    liveMode_ = false;
    Q_EMIT liveModeChanged(false);
    Q_EMIT rangeChanged();
}

void TimeAxisManager::resume() {
    if (liveMode_) {
        return;
    }
    liveMode_ = true;
    Q_EMIT liveModeChanged(true);
    Q_EMIT rangeChanged();
}

void TimeAxisManager::setRange(TimePoint start, TimePoint end) {
    if (end <= start) {
        return;  // Defensive: ignore inverted/empty ranges
    }
    const bool wasLive = liveMode_;
    pausedAt_ = end;
    duration_ = std::chrono::duration_cast<Duration>(end - start);
    liveMode_ = false;
    if (wasLive) {
        Q_EMIT liveModeChanged(false);
    }
    Q_EMIT rangeChanged();
}

void TimeAxisManager::setPreset(TimePreset preset) {
    duration_ = durationFromPreset(preset);
    const bool wasPaused = !liveMode_;
    liveMode_ = true;
    if (wasPaused) {
        Q_EMIT liveModeChanged(true);
    }
    Q_EMIT rangeChanged();
}

}  // namespace signalforge::chart
