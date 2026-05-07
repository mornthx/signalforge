// src/chart/time_axis_manager.cpp
//
// S1 ctor/dtor stubs. Full implementation lands in S2.

#include "chart/time_axis_manager.hpp"

namespace signalforge::chart {

TimeAxisManager::TimeAxisManager(QObject* parent) : QObject(parent), duration_(std::chrono::seconds(10)) {}

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

void TimeAxisManager::pan(Duration /*offset*/) {
    // S2 implementation.
}

void TimeAxisManager::zoom(double /*factor*/, TimePoint /*referencePoint*/) {
    // S2 implementation.
}

void TimeAxisManager::pause() {
    // S2 implementation.
}

void TimeAxisManager::resume() {
    // S2 implementation.
}

void TimeAxisManager::setRange(TimePoint /*start*/, TimePoint /*end*/) {
    // S2 implementation.
}

void TimeAxisManager::setPreset(TimePreset /*preset*/) {
    // S2 implementation.
}

}  // namespace signalforge::chart
