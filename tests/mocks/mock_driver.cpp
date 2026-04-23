// tests/mocks/mock_driver.cpp
#include "tests/mocks/mock_driver.hpp"

#include <chrono>

namespace signalforge::test {

using signalforge::drivers::DriverError;
using signalforge::drivers::DriverErrorCode;
using signalforge::drivers::DriverState;

MockDriver::MockDriver(QObject* parent) : signalforge::drivers::DriverInterface(parent) {}
MockDriver::~MockDriver() = default;

void MockDriver::transitionTo(DriverState newState) {
    state_.store(newState, std::memory_order_release);
    emit stateChanged(newState);
}

DriverErrorCode MockDriver::open() {
    if (preOpenFailure_ != DriverErrorCode::Success) {
        // Synchronous-validation failure: do not transition.
        return preOpenFailure_;
    }
    if (state_.load(std::memory_order_acquire) != DriverState::Idle) {
        return DriverErrorCode::NotConfigured;  // precondition
    }
    transitionTo(DriverState::Opening);
    // In a real driver the actual acquisition would run on the IO thread
    // and a separate stateChanged(Open) would arrive later. For the mock,
    // we complete synchronously.
    transitionTo(DriverState::Open);
    return DriverErrorCode::Success;
}

void MockDriver::close() {
    const auto s = state_.load(std::memory_order_acquire);
    if (s == DriverState::Idle) {
        return;  // idempotent
    }
    transitionTo(DriverState::Closing);
    transitionTo(DriverState::Idle);
}

DriverErrorCode MockDriver::start() {
    if (state_.load(std::memory_order_acquire) != DriverState::Open) {
        return DriverErrorCode::NotConfigured;
    }
    transitionTo(DriverState::Running);
    return DriverErrorCode::Success;
}

void MockDriver::stop() {
    const auto s = state_.load(std::memory_order_acquire);
    if (s != DriverState::Running) {
        return;  // idempotent
    }
    transitionTo(DriverState::Stopping);
    transitionTo(DriverState::Open);
}

DriverErrorCode MockDriver::write(const QByteArray& payload) {
    if (state_.load(std::memory_order_acquire) != DriverState::Running) {
        return DriverErrorCode::NotConfigured;
    }
    txFramesTotal_.fetch_add(1, std::memory_order_relaxed);
    txBytesTotal_.fetch_add(static_cast<std::uint64_t>(payload.size()), std::memory_order_relaxed);
    return DriverErrorCode::Success;
}

DriverState MockDriver::state() const {
    return state_.load(std::memory_order_acquire);
}

DriverErrorCode MockDriver::health() const {
    return health_.load(std::memory_order_acquire);
}

signalforge::frame::DriverStatistics MockDriver::statistics() const {
    signalforge::frame::DriverStatistics s;
    // Per-field atomic reads; cross-field consistency not guaranteed
    // (spec §4.2 atomicity note). Readers tolerate ~33 ms skew.
    s.rx.framesTotal = rxFramesTotal_.load(std::memory_order_relaxed);
    s.rx.bytesTotal = rxBytesTotal_.load(std::memory_order_relaxed);
    s.tx.framesTotal = txFramesTotal_.load(std::memory_order_relaxed);
    s.tx.bytesTotal = txBytesTotal_.load(std::memory_order_relaxed);
    s.snapshotAt = std::chrono::steady_clock::now();
    return s;
}

void MockDriver::emitFrame(const QByteArray& payload) {
    signalforge::frame::RawFrame f;
    f.sourceId = "mock";
    f.payload = payload;
    f.recvAt = std::chrono::steady_clock::now();
    f.protocolHint = "mock";
    f.sequenceNumber = rxFramesTotal_.load(std::memory_order_relaxed) + 1;
    rxFramesTotal_.fetch_add(1, std::memory_order_relaxed);
    rxBytesTotal_.fetch_add(static_cast<std::uint64_t>(payload.size()), std::memory_order_relaxed);
    emit frameReceived(f);
}

void MockDriver::injectError(DriverErrorCode code, const QString& msg) {
    DriverError err;
    err.code = code;
    err.message = msg;
    err.at = std::chrono::steady_clock::now();
    health_.store(code, std::memory_order_release);
    emit errorOccurred(err);
    transitionTo(DriverState::Error);
}

void MockDriver::setPreOpenFailure(DriverErrorCode code) {
    preOpenFailure_ = code;
}

}  // namespace signalforge::test
