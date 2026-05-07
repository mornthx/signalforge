// src/connection/connection.cpp
//
// S2: state machine + driver dispatch + signal forwarding.
// Auto-connect command sequencing lands in S7.

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
        dr::registerMetatypes();
        qRegisterMetaType<Connection::State>("signalforge::connection::Connection::State");
        return true;
    }();
    (void)once;
}

const char* describe(Connection::State s) {
    switch (s) {
    case Connection::State::Idle:
        return "Idle";
    case Connection::State::Connecting:
        return "Connecting";
    case Connection::State::Connected:
        return "Connected";
    case Connection::State::Disconnecting:
        return "Disconnecting";
    case Connection::State::Error:
        return "Error";
    }
    return "?";
}

}  // namespace

Connection::Connection(ConnectionConfig config, QObject* parent) : QObject(parent), config_(std::move(config)) {
    registerConnectionMetatypes();
    rebuildDriver();
}

Connection::~Connection() {
    // Driver must be Idle before destruction (M3 contract). If we
    // are still mid-flight, force a synchronous teardown.
    if (driver_ && driver_->state() != dr::DriverState::Idle) {
        driver_->stop();
        driver_->close();
    }
}

bool Connection::connectDriver() {
    if (state_ != State::Idle && state_ != State::Error) {
        SF_LOG_WARN("Connection({}): connectDriver rejected; state is {}", config_.id.toStdString(), describe(state_));
        return false;
    }
    if (!driver_) {
        setError(QStringLiteral("driver not constructed"));
        setState(State::Error);
        return false;
    }
    lastError_.clear();
    setState(State::Connecting);
    const auto rc = driver_->open();
    if (rc != dr::DriverErrorCode::Success) {
        setError(QStringLiteral("driver open() rejected: code %1").arg(static_cast<int>(rc)));
        setState(State::Error);
        return false;
    }
    return true;
}

bool Connection::disconnectDriver() {
    if (state_ == State::Idle) {
        return false;
    }
    if (!driver_) {
        setState(State::Idle);
        return true;
    }
    setState(State::Disconnecting);
    driver_->stop();
    driver_->close();
    // Driver close is synchronous (per M3 contract). The driver
    // emits stateChanged(Idle) which onDriverState translates back
    // to Connection::State::Idle.
    return true;
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
    if (!driver_) {
        return;
    }
    // Drivers emit signals from their IO thread (per M3 docs); use
    // QueuedConnection so slots run on the Connection's own thread.
    connect(driver_.get(), &dr::DriverInterface::stateChanged, this, &Connection::onDriverState, Qt::QueuedConnection);
    connect(driver_.get(), &dr::DriverInterface::errorOccurred, this, &Connection::onDriverError, Qt::QueuedConnection);
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

void Connection::onDriverState(dr::DriverState driverState) {
    // Translate driver lifecycle into Connection state. The
    // Connection state machine collapses driver Opening + Open into
    // Connecting (we auto-progress with start() at Open) and
    // Stopping + Closing into Disconnecting.
    switch (driverState) {
    case dr::DriverState::Idle:
        // Reached after a clean stop()+close() cycle, OR straight
        // from Error after the user disconnects. Either way we are
        // back to Idle.
        setState(State::Idle);
        break;
    case dr::DriverState::Opening:
        // Already in Connecting.
        break;
    case dr::DriverState::Open: {
        // Auto-progress to Running only if we're connecting. The
        // driver also passes through Open on the way back to Idle
        // (Stopping → Open → Closing → Idle); we must not call
        // start() during that disconnect path.
        if (state_ == State::Connecting) {
            const auto rc = driver_->start();
            if (rc != dr::DriverErrorCode::Success) {
                setError(QStringLiteral("driver start() rejected: code %1").arg(static_cast<int>(rc)));
                setState(State::Error);
            }
        }
        break;
    }
    case dr::DriverState::Running:
        setState(State::Connected);
        // S7 will trigger AutoConnectCommandSequence here. Until
        // then, emit a synthetic completion signal so consumers
        // can already observe the contract.
        if (config_.autoConnectCommands.empty()) {
            Q_EMIT autoConnectCompleted(true);
        }
        break;
    case dr::DriverState::Stopping:
    case dr::DriverState::Closing:
        if (state_ != State::Disconnecting) {
            setState(State::Disconnecting);
        }
        break;
    case dr::DriverState::Error:
        setState(State::Error);
        break;
    }
}

void Connection::onDriverError(dr::DriverError error) {
    setError(error.message);
}

}  // namespace signalforge::connection
