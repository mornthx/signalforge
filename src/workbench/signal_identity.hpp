// src/workbench/signal_identity.hpp
#pragma once

#include <QHash>
#include <QString>
#include <chrono>

namespace signalforge::workbench {

/// Signal quality, surfaced as a badge in every view. Mirrors the architecture
/// §6.1 quality enum (derived from freshness + decode errors).
enum class Quality {
    Good,       ///< fresh, decoded cleanly
    Stale,      ///< no recent update
    Uncertain,  ///< decoded but flagged (e.g. range / checksum soft-fail)
    Bad,        ///< very stale or decode error
};

/// Lowercase token for a quality (`color.quality.<name>` / QSS class).
[[nodiscard]] QString qualityName(Quality quality);

/// Derive a quality from the latest value's age. `staleAfter` / `badAfter` are
/// caller-chosen thresholds (typically a few signal periods). `decodeError`
/// forces a degraded state regardless of age. `Uncertain` is reserved for the
/// soft-fail path wired in a later phase.
[[nodiscard]] Quality qualityFromAge(std::chrono::nanoseconds age, std::chrono::nanoseconds staleAfter,
                                     std::chrono::nanoseconds badAfter, bool decodeError = false);

/// Stable per-signal visual identity: assigns each signal a **palette color
/// index** (0 .. kPaletteSize-1) on first sighting, in first-seen order,
/// wrapping. The actual color is resolved from the theme tokens
/// (`color.signal.<index>`) by the view, so this service is theme-independent
/// and is the single source of truth for "what color is this signal" across
/// Raw / Parsed / Dashboard.
class SignalIdentity {
public:
    static constexpr int kPaletteSize = 8;  ///< matches color.signal.0..7

    /// Palette index for `signalId`, assigning a new one if unseen. Stable for
    /// the lifetime of this object.
    [[nodiscard]] int colorIndex(const QString& signalId);

    /// Whether `signalId` already has an assigned index (no assignment).
    [[nodiscard]] bool isKnown(const QString& signalId) const;

    /// Number of signals assigned so far.
    [[nodiscard]] int count() const;

private:
    QHash<QString, int> indices_;
    int next_ = 0;
};

}  // namespace signalforge::workbench
