#pragma once

// GENERATED FROM resources/styles/tokens.json — DO NOT EDIT MANUALLY
// Generator: tools/generate_style_assets.py
// Run `--check` to verify freshness (M16 R15 / H12)
// tokens.json version 1.0

// Consume design tokens from C++ widget / styling code. Manifesto
// principles + token rationale: see resources/styles/tokens.json
// `_manifesto_refs` section.

#include <QColor>
#include <QString>

namespace signalforge::tokens::light {

// ----- Color hex strings (string-form for QSS interop) -------------

inline constexpr const char* kBgElevatedHex = "#f5f5f4";
inline constexpr const char* kBgPrimaryHex = "#fbfbfa";
inline constexpr const char* kBgSurfaceHex = "#ffffff";
inline constexpr const char* kBorderHex = "#d6d6d4";
inline constexpr const char* kBorderFocusHex = "#3b7ddd";
inline constexpr const char* kModeEndedHex = "#5a5d63";
inline constexpr const char* kModeLiveHex = "#2d8a3e";
inline constexpr const char* kModePausedHex = "#d4a72c";
inline constexpr const char* kModeRecordingHex = "#c8392a";
inline constexpr const char* kModeReplayHex = "#3b7ddd";
inline constexpr const char* kSeverityErrorHex = "#c8392a";
inline constexpr const char* kSeverityInfoHex = "#5a5d63";
inline constexpr const char* kSeverityWarningHex = "#d4a72c";
inline constexpr const char* kSignal0Hex = "#2d6cb3";
inline constexpr const char* kSignal1Hex = "#c8392a";
inline constexpr const char* kSignal2Hex = "#2d8a3e";
inline constexpr const char* kSignal3Hex = "#d4a72c";
inline constexpr const char* kSignal4Hex = "#7e3eb3";
inline constexpr const char* kSignal5Hex = "#2da3a3";
inline constexpr const char* kSignal6Hex = "#d4622c";
inline constexpr const char* kSignal7Hex = "#5a5d63";
inline constexpr const char* kStatusConnectedHex = "#2d8a3e";
inline constexpr const char* kStatusConnectingHex = "#d4a72c";
inline constexpr const char* kStatusDisconnectingHex = "#d4a72c";
inline constexpr const char* kStatusErrorHex = "#c8392a";
inline constexpr const char* kStatusIdleHex = "#5a5d63";
inline constexpr const char* kTextDisabledHex = "#9ea1a7";
inline constexpr const char* kTextPrimaryHex = "#1a1d23";
inline constexpr const char* kTextSecondaryHex = "#5a5d63";

// ----- Color QColor accessors (inline; QColor not constexpr) ------

inline QColor bgElevated() { return QColor(QString::fromLatin1("#f5f5f4")); }
inline QColor bgPrimary() { return QColor(QString::fromLatin1("#fbfbfa")); }
inline QColor bgSurface() { return QColor(QString::fromLatin1("#ffffff")); }
inline QColor border() { return QColor(QString::fromLatin1("#d6d6d4")); }
inline QColor borderFocus() { return QColor(QString::fromLatin1("#3b7ddd")); }
inline QColor modeEnded() { return QColor(QString::fromLatin1("#5a5d63")); }
inline QColor modeLive() { return QColor(QString::fromLatin1("#2d8a3e")); }
inline QColor modePaused() { return QColor(QString::fromLatin1("#d4a72c")); }
inline QColor modeRecording() { return QColor(QString::fromLatin1("#c8392a")); }
inline QColor modeReplay() { return QColor(QString::fromLatin1("#3b7ddd")); }
inline QColor severityError() { return QColor(QString::fromLatin1("#c8392a")); }
inline QColor severityInfo() { return QColor(QString::fromLatin1("#5a5d63")); }
inline QColor severityWarning() { return QColor(QString::fromLatin1("#d4a72c")); }
inline QColor signal0() { return QColor(QString::fromLatin1("#2d6cb3")); }
inline QColor signal1() { return QColor(QString::fromLatin1("#c8392a")); }
inline QColor signal2() { return QColor(QString::fromLatin1("#2d8a3e")); }
inline QColor signal3() { return QColor(QString::fromLatin1("#d4a72c")); }
inline QColor signal4() { return QColor(QString::fromLatin1("#7e3eb3")); }
inline QColor signal5() { return QColor(QString::fromLatin1("#2da3a3")); }
inline QColor signal6() { return QColor(QString::fromLatin1("#d4622c")); }
inline QColor signal7() { return QColor(QString::fromLatin1("#5a5d63")); }
inline QColor statusConnected() { return QColor(QString::fromLatin1("#2d8a3e")); }
inline QColor statusConnecting() { return QColor(QString::fromLatin1("#d4a72c")); }
inline QColor statusDisconnecting() { return QColor(QString::fromLatin1("#d4a72c")); }
inline QColor statusError() { return QColor(QString::fromLatin1("#c8392a")); }
inline QColor statusIdle() { return QColor(QString::fromLatin1("#5a5d63")); }
inline QColor textDisabled() { return QColor(QString::fromLatin1("#9ea1a7")); }
inline QColor textPrimary() { return QColor(QString::fromLatin1("#1a1d23")); }
inline QColor textSecondary() { return QColor(QString::fromLatin1("#5a5d63")); }

// ----- Font ---------------------------------------------------------

inline constexpr const char* kFontFamilySans = "Inter";
inline constexpr const char* kFontFamilyMono = "JetBrains Mono";
inline constexpr int kFontSizeDisplay = 18;
inline constexpr int kFontSizeHeading = 14;
inline constexpr int kFontSizeBody    = 12;
inline constexpr int kFontSizeCaption = 11;
inline constexpr int kFontSizeMono    = 12;
inline constexpr int kFontWeightRegular = 400;
inline constexpr int kFontWeightMedium  = 500;
inline constexpr int kFontWeightBold    = 700;

// ----- Spacing (px) -------------------------------------------------

inline constexpr int kSpacingXs = 4;
inline constexpr int kSpacingSm = 8;
inline constexpr int kSpacingMd = 16;
inline constexpr int kSpacingLg = 24;
inline constexpr int kSpacingXl = 32;

// ----- Icon sizes (px) ----------------------------------------------

inline constexpr int kIconSm = 16;
inline constexpr int kIconMd = 20;
inline constexpr int kIconLg = 32;

}  // namespace signalforge::tokens::light
