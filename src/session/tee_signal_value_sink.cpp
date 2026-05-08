// src/session/tee_signal_value_sink.cpp
#include "session/tee_signal_value_sink.hpp"

#include <algorithm>

namespace signalforge::session {

void TeeSignalValueSink::addSink(signalforge::decoder::SignalValueSink* sink) {
    if (sink == nullptr) {
        return;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    if (std::find(sinks_.begin(), sinks_.end(), sink) == sinks_.end()) {
        sinks_.push_back(sink);
    }
}

void TeeSignalValueSink::removeSink(signalforge::decoder::SignalValueSink* sink) {
    std::lock_guard<std::mutex> lock(mutex_);
    sinks_.erase(std::remove(sinks_.begin(), sinks_.end(), sink), sinks_.end());
}

std::size_t TeeSignalValueSink::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return sinks_.size();
}

void TeeSignalValueSink::onSignal(std::chrono::steady_clock::time_point timestamp, const QString& signalId,
                                  const signalforge::decoder::SignalValue& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto* sink : sinks_) {
        sink->onSignal(timestamp, signalId, value);
    }
}

void TeeSignalValueSink::onSignalsRegistered(const QString& driverId,
                                             const std::vector<signalforge::decoder::SignalMetadata>& signalsList) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto* sink : sinks_) {
        sink->onSignalsRegistered(driverId, signalsList);
    }
}

void TeeSignalValueSink::onSignalsUnregistered(const QString& driverId) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto* sink : sinks_) {
        sink->onSignalsUnregistered(driverId);
    }
}

}  // namespace signalforge::session
