// tests/unit/decode/decoder_test.cpp
#include "decode/decoder_interface.hpp"
#include "decode/logging_signal_value_sink.hpp"

#include <catch2/catch_test_macros.hpp>
#include <chrono>

using signalforge::decoder::LoggingSignalValueSink;
using signalforge::decoder::SignalMetadata;
using signalforge::decoder::SignalType;
using signalforge::decoder::SignalValue;

TEST_CASE("LoggingSignalValueSink: construction starts with zero counters", "[decoder][logging_sink]") {
    LoggingSignalValueSink sink;
    REQUIRE(sink.signalsReceived() == 0);
    REQUIRE(sink.signalsByType(SignalType::Bool) == 0);
    REQUIRE(sink.signalsByType(SignalType::Int64) == 0);
    REQUIRE(sink.signalsByType(SignalType::Double) == 0);
    REQUIRE(sink.signalsByType(SignalType::String) == 0);
    REQUIRE(sink.registrationsReceived() == 0);
    REQUIRE(sink.unregistrationsReceived() == 0);
}

TEST_CASE("LoggingSignalValueSink: onSignal records each variant type", "[decoder][logging_sink]") {
    LoggingSignalValueSink sink;
    const auto ts = std::chrono::steady_clock::now();

    sink.onSignal(ts, QStringLiteral("id1"), SignalValue{true});
    sink.onSignal(ts, QStringLiteral("id2"), SignalValue{std::int64_t{42}});
    sink.onSignal(ts, QStringLiteral("id3"), SignalValue{3.14});
    sink.onSignal(ts, QStringLiteral("id4"), SignalValue{QStringLiteral("text")});
    sink.onSignal(ts, QStringLiteral("id5"), SignalValue{false});

    REQUIRE(sink.signalsReceived() == 5);
    REQUIRE(sink.signalsByType(SignalType::Bool) == 2);
    REQUIRE(sink.signalsByType(SignalType::Int64) == 1);
    REQUIRE(sink.signalsByType(SignalType::Double) == 1);
    REQUIRE(sink.signalsByType(SignalType::String) == 1);
}

TEST_CASE("LoggingSignalValueSink: registration and unregistration callbacks count", "[decoder][logging_sink]") {
    LoggingSignalValueSink sink;
    std::vector<SignalMetadata> meta;
    meta.push_back({QStringLiteral("d1/a"), QStringLiteral("a"), QStringLiteral(""), SignalType::Int64, std::nullopt,
                    std::nullopt, std::nullopt});

    sink.onSignalsRegistered(QStringLiteral("d1"), meta);
    REQUIRE(sink.registrationsReceived() == 1);

    sink.onSignalsUnregistered(QStringLiteral("d1"));
    REQUIRE(sink.unregistrationsReceived() == 1);
}

TEST_CASE("LoggingSignalValueSink: resetCounters zeroes all state", "[decoder][logging_sink]") {
    LoggingSignalValueSink sink;
    const auto ts = std::chrono::steady_clock::now();
    sink.onSignal(ts, QStringLiteral("x"), SignalValue{std::int64_t{1}});
    sink.onSignalsRegistered(QStringLiteral("d"), {});
    REQUIRE(sink.signalsReceived() == 1);
    REQUIRE(sink.registrationsReceived() == 1);

    sink.resetCounters();
    REQUIRE(sink.signalsReceived() == 0);
    REQUIRE(sink.signalsByType(SignalType::Int64) == 0);
    REQUIRE(sink.registrationsReceived() == 0);
    REQUIRE(sink.unregistrationsReceived() == 0);
}
