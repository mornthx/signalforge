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

int SignalIdentity::colorIndex(const QString& signalId) {
    if (const auto it = indices_.constFind(signalId); it != indices_.constEnd()) {
        return it.value();
    }
    const int index = next_ % kPaletteSize;
    ++next_;
    indices_.insert(signalId, index);
    return index;
}

bool SignalIdentity::isKnown(const QString& signalId) const {
    return indices_.contains(signalId);
}

int SignalIdentity::count() const {
    return static_cast<int>(indices_.size());
}

}  // namespace signalforge::workbench
