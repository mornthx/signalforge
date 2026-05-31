// src/workbench/signal_identity.hpp
#pragma once

#include <QColor>
#include <QHash>
#include <QString>
#include <chrono>
#include <optional>

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

/// Stable visual identity, the single source of truth for "what color is this
/// signal" across Raw / Parsed / Dashboard. Two layers:
///
///   1. **Per-driver palette index** (0 .. kPaletteSize-1), assigned to each
///      *driver* (the `<driver>/<field>` prefix) on first sighting, in
///      first-seen order, wrapping. So every signal from one driver shares one
///      colour by default — the bring-up operator groups a device's signals at
///      a glance. The index resolves to a theme token (`color.signal.<index>`)
///      in the view, so this layer is theme-independent.
///   2. **Per-signal override** — an explicit colour the user picks (right-click
///      ▸ Set colour…) for an individual signal, taking precedence over its
///      driver default. Theme-independent by design (the user chose it).
///
/// The view/app resolves a signal's colour as: override if present, else the
/// driver's palette colour.
class SignalIdentity {
public:
    static constexpr int kPaletteSize = 8;  ///< matches color.signal.0..7

    /// The driver key for a signal id: the prefix before the first '/', or the
    /// whole id when it has no field part. Signals sharing a key share a colour.
    [[nodiscard]] static QString driverKeyOf(const QString& signalId);

    /// Palette index for `signalId`'s **driver**, assigning a new one to that
    /// driver if unseen. Stable for the lifetime of this object.
    [[nodiscard]] int colorIndex(const QString& signalId);

    /// The user's explicit colour override for `signalId`, if any.
    [[nodiscard]] std::optional<QColor> overrideColor(const QString& signalId) const;

    /// Set / clear a per-signal colour override.
    void setOverrideColor(const QString& signalId, const QColor& color);
    void clearOverrideColor(const QString& signalId);

    /// Whether `signalId` has a per-signal override.
    [[nodiscard]] bool hasOverride(const QString& signalId) const;

    /// Whether `signalId`'s driver already has an assigned index (no assignment).
    [[nodiscard]] bool isKnown(const QString& signalId) const;

    /// Number of distinct drivers assigned so far.
    [[nodiscard]] int count() const;

private:
    QHash<QString, int> driverIndices_;  ///< driver key → palette index
    QHash<QString, QColor> overrides_;   ///< signal id → explicit colour
    int next_ = 0;
};

}  // namespace signalforge::workbench
