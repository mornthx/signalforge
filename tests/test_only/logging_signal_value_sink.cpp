// tests/test_only/logging_signal_value_sink.cpp
#include "tests/test_only/logging_signal_value_sink.hpp"

#include "observability/logging.hpp"

namespace signalforge::decoder {

namespace {

QString formatValue(const SignalValue& v) {
    return std::visit(
        [](const auto& arg) -> QString {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, bool>) {
                return arg ? QStringLiteral("true") : QStringLiteral("false");
            } else if constexpr (std::is_same_v<T, std::int64_t>) {
                return QString::number(arg);
            } else if constexpr (std::is_same_v<T, double>) {
                return QString::number(arg, 'g', 10);
            } else if constexpr (std::is_same_v<T, QString>) {
                return QStringLiteral("\"") + arg + QStringLiteral("\"");
            } else {
                return QStringLiteral("?");
            }
        },
        v);
}

std::size_t typeIndex(SignalType t) noexcept {
    return static_cast<std::size_t>(t);
}

}  // namespace

LoggingSignalValueSink::LoggingSignalValueSink() {
    for (auto& c : perType_) {
        c.store(0, std::memory_order_relaxed);
    }
}

void LoggingSignalValueSink::onSignal(std::chrono::steady_clock::time_point /*timestamp*/, const QString& signalId,
                                      const SignalValue& value) {
    signalsReceived_.fetch_add(1, std::memory_order_relaxed);
    perType_[value.index()].fetch_add(1, std::memory_order_relaxed);
    SF_LOG_INFO("LoggingSignalValueSink: {} = {}", signalId.toStdString(), formatValue(value).toStdString());
}

void LoggingSignalValueSink::onSignalsRegistered(const QString& driverId,
                                                 const std::vector<SignalMetadata>& signalsList) {
    registrationsReceived_.fetch_add(1, std::memory_order_relaxed);
    SF_LOG_INFO("LoggingSignalValueSink: driver '{}' registered {} signal(s)", driverId.toStdString(),
                signalsList.size());
}

void LoggingSignalValueSink::onSignalsUnregistered(const QString& driverId) {
    unregistrationsReceived_.fetch_add(1, std::memory_order_relaxed);
    SF_LOG_INFO("LoggingSignalValueSink: driver '{}' unregistered", driverId.toStdString());
}

std::uint64_t LoggingSignalValueSink::signalsReceived() const noexcept {
    return signalsReceived_.load(std::memory_order_relaxed);
}

std::uint64_t LoggingSignalValueSink::signalsByType(SignalType t) const noexcept {
    return perType_[typeIndex(t)].load(std::memory_order_relaxed);
}

std::uint64_t LoggingSignalValueSink::registrationsReceived() const noexcept {
    return registrationsReceived_.load(std::memory_order_relaxed);
}

std::uint64_t LoggingSignalValueSink::unregistrationsReceived() const noexcept {
    return unregistrationsReceived_.load(std::memory_order_relaxed);
}

void LoggingSignalValueSink::resetCounters() noexcept {
    signalsReceived_.store(0, std::memory_order_relaxed);
    registrationsReceived_.store(0, std::memory_order_relaxed);
    unregistrationsReceived_.store(0, std::memory_order_relaxed);
    for (auto& c : perType_) {
        c.store(0, std::memory_order_relaxed);
    }
}

}  // namespace signalforge::decoder
