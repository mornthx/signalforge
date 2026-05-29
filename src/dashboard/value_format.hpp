// src/dashboard/value_format.hpp
#pragma once

#include "decode/decoder_interface.hpp"

#include <QString>
#include <cstdint>
#include <limits>
#include <variant>

namespace signalforge::dashboard {

/// Numeric projection of a SignalValue. Returns NaN for QString
/// variants so callers can skip them from min/max tracking.
[[nodiscard]] inline double valueToDouble(const signalforge::decoder::SignalValue& v) noexcept {
    if (const auto* d = std::get_if<double>(&v); d != nullptr) {
        return *d;
    }
    if (const auto* i = std::get_if<std::int64_t>(&v); i != nullptr) {
        return static_cast<double>(*i);
    }
    if (const auto* b = std::get_if<bool>(&v); b != nullptr) {
        return *b ? 1.0 : 0.0;
    }
    return std::numeric_limits<double>::quiet_NaN();
}

/// Human-readable text for a SignalValue. `decimals` applies to
/// doubles only; ints print whole, bools as true/false, strings
/// verbatim.
[[nodiscard]] inline QString formatValue(const signalforge::decoder::SignalValue& v, int decimals) {
    if (const auto* d = std::get_if<double>(&v); d != nullptr) {
        return QString::number(*d, 'f', decimals);
    }
    if (const auto* i = std::get_if<std::int64_t>(&v); i != nullptr) {
        return QString::number(static_cast<qlonglong>(*i));
    }
    if (const auto* b = std::get_if<bool>(&v); b != nullptr) {
        return *b ? QStringLiteral("true") : QStringLiteral("false");
    }
    if (const auto* s = std::get_if<QString>(&v); s != nullptr) {
        return *s;
    }
    return QStringLiteral("—");
}

}  // namespace signalforge::dashboard
