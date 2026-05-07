// src/connection/connection.cpp
//
// S1: minimal scaffolding. The state machine, error wiring, and
// auto-connect command sequencing land in S2/S7. S1 establishes
// the freeze-surface header and the constructor's driver-factory
// dispatch so the static lib links.

#include "connection/connection.hpp"

#include "drivers/replay_driver.hpp"
#include "drivers/serial_driver.hpp"
#include "drivers/tcp_driver.hpp"
#include "drivers/udp_driver.hpp"
#include "observability/logging.hpp"

namespace signalforge::connection {

namespace dr = signalforge::drivers;

namespace {

void registerConnectionMetatypes() {
    static bool once = [] {
        qRegisterMetaType<Connection::State>("signalforge::connection::Connection::State");
        return true;
    }();
    (void)once;
}

}  // namespace

Connection::Connection(ConnectionConfig config, QObject* parent) : QObject(parent), config_(std::move(config)) {
    registerConnectionMetatypes();
    rebuildDriver();
}

Connection::~Connection() = default;

bool Connection::connectDriver() {
    // S2 fills in the actual state-machine + driver dispatch.
    SF_LOG_DEBUG("Connection::connectDriver() stub for {}", config_.id.toStdString());
    return false;
}

bool Connection::disconnectDriver() {
    // S2 fills in.
    SF_LOG_DEBUG("Connection::disconnectDriver() stub for {}", config_.id.toStdString());
    return false;
}

Connection::State Connection::state() const noexcept {
    return state_;
}

const QString& Connection::lastError() const noexcept {
    return lastError_;
}

const ConnectionConfig& Connection::config() const noexcept {
    return config_;
}

dr::DriverInterface* Connection::driver() const noexcept {
    return driver_.get();
}

bool Connection::setConfig(const ConnectionConfig& newConfig) {
    if (state_ != State::Idle) {
        return false;
    }
    config_ = newConfig;
    rebuildDriver();
    return true;
}

void Connection::rebuildDriver() {
    driver_.reset();
    switch (config_.driverType) {
    case DriverType::Serial:
        driver_ = std::make_unique<dr::SerialDriver>(std::get<dr::SerialConfig>(config_.driverConfig));
        break;
    case DriverType::Tcp:
        driver_ = std::make_unique<dr::TcpDriver>(std::get<dr::TcpConfig>(config_.driverConfig));
        break;
    case DriverType::Udp:
        driver_ = std::make_unique<dr::UdpDriver>(std::get<dr::UdpConfig>(config_.driverConfig));
        break;
    case DriverType::Replay:
        driver_ = std::make_unique<dr::ReplayDriver>(std::get<dr::ReplayConfig>(config_.driverConfig));
        break;
    }
    wireDriverSignals();
}

void Connection::wireDriverSignals() {
    // S2 fills in: connect driver_->stateChanged → onDriverState etc.
}

void Connection::setState(State next) {
    if (state_ == next) {
        return;
    }
    state_ = next;
    Q_EMIT stateChanged(state_);
}

void Connection::setError(const QString& message) {
    lastError_ = message;
    Q_EMIT errorOccurred(message);
}

void Connection::onDriverState(dr::DriverState /*driverState*/) {
    // S2: translate DriverState → Connection::State.
}

void Connection::onDriverError(dr::DriverError /*error*/) {
    // S2: forward + transition.
}

}  // namespace signalforge::connection
