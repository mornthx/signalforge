// src/chart/time_axis_manager.hpp
#pragma once

#include <QObject>
#include <chrono>

namespace signalforge::chart {

/// Single-source-of-truth time-axis state shared across all charts
/// (decision M8.3 Option T: global time axis).
///
/// Holds the visible range (`visibleStart`, `visibleEnd`), the
/// `liveMode` flag, and (when paused) the anchor `pausedAt`. Pan /
/// zoom / pause / resume go through this manager; charts react to
/// `rangeChanged` and `liveModeChanged` Qt signals.
///
/// Threading: lives on the main thread. All mutations and reads
/// happen on the main thread; cross-thread access is not supported
/// in V1.
///
/// Freeze scope: this class is frozen at M8 close. See spec §6.1.
class TimeAxisManager : public QObject {
    Q_OBJECT

public:
    using TimePoint = std::chrono::steady_clock::time_point;
    using Duration = std::chrono::nanoseconds;

    /// Time-range presets exposed in the toolbar.
    enum class TimePreset { Sec1, Sec10, Min1, Min10, Hour1 };

    explicit TimeAxisManager(QObject* parent = nullptr);
    ~TimeAxisManager() override;

    TimeAxisManager(const TimeAxisManager&) = delete;
    TimeAxisManager& operator=(const TimeAxisManager&) = delete;
    TimeAxisManager(TimeAxisManager&&) = delete;
    TimeAxisManager& operator=(TimeAxisManager&&) = delete;

    /// Visible range start. In live mode, dynamically computed as
    /// `visibleEnd() - visibleDuration()`.
    [[nodiscard]] TimePoint visibleStart() const noexcept;

    /// Visible range end. In live mode, returns
    /// `std::chrono::steady_clock::now()`. In paused mode, returns
    /// the anchored `pausedAt_`.
    [[nodiscard]] TimePoint visibleEnd() const noexcept;

    /// Visible duration (`end - start`).
    [[nodiscard]] Duration visibleDuration() const noexcept;

    /// `true` while `visibleEnd()` tracks `now()`.
    [[nodiscard]] bool liveMode() const noexcept;

    /// Pan: shift the visible range by `offset`. Transitions to
    /// paused mode if currently live.
    void pan(Duration offset);

    /// Zoom: scale visible duration around `referencePoint` by
    /// `factor` (>1 zooms out, <1 zooms in). Transitions to paused
    /// mode if currently live.
    void zoom(double factor, TimePoint referencePoint);

    /// Pause live mode at the current `visibleEnd()`.
    void pause();

    /// Resume live mode. `visibleEnd()` resumes tracking `now()`.
    void resume();

    /// Set visible range explicitly. Transitions to paused mode.
    void setRange(TimePoint start, TimePoint end);

    /// Set a preset duration ending at `now()` (live mode).
    void setPreset(TimePreset preset);

Q_SIGNALS:
    void rangeChanged();
    void liveModeChanged(bool live);

private:
    Duration duration_;
    TimePoint pausedAt_{};
    bool liveMode_ = true;
};

}  // namespace signalforge::chart
