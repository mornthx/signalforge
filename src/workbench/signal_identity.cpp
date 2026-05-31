// src/workbench/signal_identity.cpp

#include "workbench/signal_identity.hpp"

namespace signalforge::workbench {

QString qualityName(Quality quality) {
    switch (quality) {
    case Quality::Good:
        return QStringLiteral("good");
    case Quality::Stale:
        return QStringLiteral("stale");
    case Quality::Uncertain:
        return QStringLiteral("uncertain");
    case Quality::Bad:
        return QStringLiteral("bad");
    }
    return QStringLiteral("uncertain");
}

Quality qualityFromAge(std::chrono::nanoseconds age, std::chrono::nanoseconds staleAfter,
                       std::chrono::nanoseconds badAfter, bool decodeError) {
    if (decodeError) {
        return Quality::Bad;
    }
    if (age >= badAfter) {
        return Quality::Bad;
    }
    if (age >= staleAfter) {
        return Quality::Stale;
    }
    return Quality::Good;
}

QString SignalIdentity::driverKeyOf(const QString& signalId) {
    const int slash = signalId.indexOf(QLatin1Char('/'));
    return slash > 0 ? signalId.left(slash) : signalId;
}

int SignalIdentity::colorIndex(const QString& signalId) {
    const QString key = driverKeyOf(signalId);
    if (const auto it = driverIndices_.constFind(key); it != driverIndices_.constEnd()) {
        return it.value();
    }
    const int index = next_ % kPaletteSize;
    ++next_;
    driverIndices_.insert(key, index);
    return index;
}

std::optional<QColor> SignalIdentity::overrideColor(const QString& signalId) const {
    if (const auto it = overrides_.constFind(signalId); it != overrides_.constEnd()) {
        return it.value();
    }
    return std::nullopt;
}

void SignalIdentity::setOverrideColor(const QString& signalId, const QColor& color) {
    overrides_.insert(signalId, color);
}

void SignalIdentity::clearOverrideColor(const QString& signalId) {
    overrides_.remove(signalId);
}

bool SignalIdentity::hasOverride(const QString& signalId) const {
    return overrides_.contains(signalId);
}

bool SignalIdentity::isKnown(const QString& signalId) const {
    return driverIndices_.contains(driverKeyOf(signalId));
}

int SignalIdentity::count() const {
    return static_cast<int>(driverIndices_.size());
}

}  // namespace signalforge::workbench
