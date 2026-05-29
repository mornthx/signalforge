// src/dashboard/panel_types.hpp
#pragma once

#include <QString>
#include <QStringList>
#include <optional>

namespace signalforge::dashboard {

/// Kind of widget a panel renders. P0 ships Plot / Numeric / State;
/// Table, Bar, Gauge land in later phases (see
/// docs/v0.3/dashboard-interaction-design.md §3).
enum class PanelType {
    Plot,     ///< Time-series trend (wraps the legacy chart for P0).
    Numeric,  ///< Big current value + unit (single signal).
    State,    ///< Boolean ●/○ or string/enum state (single signal).
};

/// Stable lowercase token for a panel type (config / diagnostics).
[[nodiscard]] QString panelTypeName(PanelType type);

/// Inverse of `panelTypeName`; nullopt for unknown tokens.
[[nodiscard]] std::optional<PanelType> panelTypeFromName(const QString& name);

/// Per-panel configuration. Zero-config by default: empty/unset fields
/// are derived from the bound signal's metadata and observed data at
/// render time. Any field may be overridden per panel (design §2.1).
struct PanelConfig {
    QString id;                           ///< Stable unique id (e.g. "panel-1").
    PanelType type = PanelType::Numeric;  ///< Widget kind.
    QString title;                        ///< Empty → derived from signal metadata.
    QStringList signalIds;                ///< Numeric/State: 1 signal; Plot: N.
    std::optional<double> rangeMin;       ///< Unset → use observed minimum.
    std::optional<double> rangeMax;       ///< Unset → use observed maximum.
    QString unitOverride;                 ///< Empty → use SignalMetadata.unit.
    int decimals = 3;                     ///< Numeric display precision.
};

}  // namespace signalforge::dashboard
