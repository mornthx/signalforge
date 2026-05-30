#pragma once

// GENERATED FROM resources/styles/tokens.json — DO NOT EDIT MANUALLY
// Generator: tools/generate_style_assets.py
// Run `--check` to verify freshness (M16 R15 / H12)
// tokens.json version 1.2

// Consume design tokens from C++ widget / styling code. Manifesto
// principles + token rationale: see resources/styles/tokens.json
// `_manifesto_refs` section.

#include <QColor>
#include <QString>

namespace signalforge::tokens::light {

// ----- Color hex strings (string-form for QSS interop) -------------

inline constexpr const char* kAccentHex = "#3b7ddd";
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

inline QColor accent() { return QColor(QString::fromLatin1("#3b7ddd")); }
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

namespace signalforge::tokens::dark {

// ----- Color hex strings (string-form for QSS interop) -------------

inline constexpr const char* kAccentHex = "#7fb2ff";
inline constexpr const char* kBgElevatedHex = "#272b34";
inline constexpr const char* kBgPrimaryHex = "#15171c";
inline constexpr const char* kBgSurfaceHex = "#1d2027";
inline constexpr const char* kBorderHex = "#3b414d";
inline constexpr const char* kBorderFocusHex = "#7fb2ff";
inline constexpr const char* kModeEndedHex = "#c2c7d0";
inline constexpr const char* kModeLiveHex = "#68d979";
inline constexpr const char* kModePausedHex = "#ffd166";
inline constexpr const char* kModeRecordingHex = "#ff6b5f";
inline constexpr const char* kModeReplayHex = "#7fb2ff";
inline constexpr const char* kSeverityErrorHex = "#ff6b5f";
inline constexpr const char* kSeverityInfoHex = "#c2c7d0";
inline constexpr const char* kSeverityWarningHex = "#ffd166";
inline constexpr const char* kSignal0Hex = "#6cb6ff";
inline constexpr const char* kSignal1Hex = "#ff6b5f";
inline constexpr const char* kSignal2Hex = "#68d979";
inline constexpr const char* kSignal3Hex = "#ffd166";
inline constexpr const char* kSignal4Hex = "#c594ff";
inline constexpr const char* kSignal5Hex = "#5de0e6";
inline constexpr const char* kSignal6Hex = "#ff9f5a";
inline constexpr const char* kSignal7Hex = "#d6dae2";
inline constexpr const char* kStatusConnectedHex = "#68d979";
inline constexpr const char* kStatusConnectingHex = "#ffd166";
inline constexpr const char* kStatusDisconnectingHex = "#ffd166";
inline constexpr const char* kStatusErrorHex = "#ff6b5f";
inline constexpr const char* kStatusIdleHex = "#c2c7d0";
inline constexpr const char* kTextDisabledHex = "#777f8d";
inline constexpr const char* kTextPrimaryHex = "#f4f6f8";
inline constexpr const char* kTextSecondaryHex = "#c2c7d0";

// ----- Color QColor accessors (inline; QColor not constexpr) ------

inline QColor accent() { return QColor(QString::fromLatin1("#7fb2ff")); }
inline QColor bgElevated() { return QColor(QString::fromLatin1("#272b34")); }
inline QColor bgPrimary() { return QColor(QString::fromLatin1("#15171c")); }
inline QColor bgSurface() { return QColor(QString::fromLatin1("#1d2027")); }
inline QColor border() { return QColor(QString::fromLatin1("#3b414d")); }
inline QColor borderFocus() { return QColor(QString::fromLatin1("#7fb2ff")); }
inline QColor modeEnded() { return QColor(QString::fromLatin1("#c2c7d0")); }
inline QColor modeLive() { return QColor(QString::fromLatin1("#68d979")); }
inline QColor modePaused() { return QColor(QString::fromLatin1("#ffd166")); }
inline QColor modeRecording() { return QColor(QString::fromLatin1("#ff6b5f")); }
inline QColor modeReplay() { return QColor(QString::fromLatin1("#7fb2ff")); }
inline QColor severityError() { return QColor(QString::fromLatin1("#ff6b5f")); }
inline QColor severityInfo() { return QColor(QString::fromLatin1("#c2c7d0")); }
inline QColor severityWarning() { return QColor(QString::fromLatin1("#ffd166")); }
inline QColor signal0() { return QColor(QString::fromLatin1("#6cb6ff")); }
inline QColor signal1() { return QColor(QString::fromLatin1("#ff6b5f")); }
inline QColor signal2() { return QColor(QString::fromLatin1("#68d979")); }
inline QColor signal3() { return QColor(QString::fromLatin1("#ffd166")); }
inline QColor signal4() { return QColor(QString::fromLatin1("#c594ff")); }
inline QColor signal5() { return QColor(QString::fromLatin1("#5de0e6")); }
inline QColor signal6() { return QColor(QString::fromLatin1("#ff9f5a")); }
inline QColor signal7() { return QColor(QString::fromLatin1("#d6dae2")); }
inline QColor statusConnected() { return QColor(QString::fromLatin1("#68d979")); }
inline QColor statusConnecting() { return QColor(QString::fromLatin1("#ffd166")); }
inline QColor statusDisconnecting() { return QColor(QString::fromLatin1("#ffd166")); }
inline QColor statusError() { return QColor(QString::fromLatin1("#ff6b5f")); }
inline QColor statusIdle() { return QColor(QString::fromLatin1("#c2c7d0")); }
inline QColor textDisabled() { return QColor(QString::fromLatin1("#777f8d")); }
inline QColor textPrimary() { return QColor(QString::fromLatin1("#f4f6f8")); }
inline QColor textSecondary() { return QColor(QString::fromLatin1("#c2c7d0")); }

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

}  // namespace signalforge::tokens::dark

namespace signalforge::tokens::high_contrast {

// ----- Color hex strings (string-form for QSS interop) -------------

inline constexpr const char* kAccentHex = "#00e5ff";
inline constexpr const char* kBgElevatedHex = "#101010";
inline constexpr const char* kBgPrimaryHex = "#000000";
inline constexpr const char* kBgSurfaceHex = "#050505";
inline constexpr const char* kBorderHex = "#ffffff";
inline constexpr const char* kBorderFocusHex = "#00e5ff";
inline constexpr const char* kModeEndedHex = "#ffffff";
inline constexpr const char* kModeLiveHex = "#00ff66";
inline constexpr const char* kModePausedHex = "#ffff00";
inline constexpr const char* kModeRecordingHex = "#ff4040";
inline constexpr const char* kModeReplayHex = "#00e5ff";
inline constexpr const char* kSeverityErrorHex = "#ff4040";
inline constexpr const char* kSeverityInfoHex = "#ffffff";
inline constexpr const char* kSeverityWarningHex = "#ffff00";
inline constexpr const char* kSignal0Hex = "#00e5ff";
inline constexpr const char* kSignal1Hex = "#ff4040";
inline constexpr const char* kSignal2Hex = "#00ff66";
inline constexpr const char* kSignal3Hex = "#ffff00";
inline constexpr const char* kSignal4Hex = "#ff66ff";
inline constexpr const char* kSignal5Hex = "#66ffff";
inline constexpr const char* kSignal6Hex = "#ff9f00";
inline constexpr const char* kSignal7Hex = "#ffffff";
inline constexpr const char* kStatusConnectedHex = "#00ff66";
inline constexpr const char* kStatusConnectingHex = "#ffff00";
inline constexpr const char* kStatusDisconnectingHex = "#ffff00";
inline constexpr const char* kStatusErrorHex = "#ff4040";
inline constexpr const char* kStatusIdleHex = "#ffffff";
inline constexpr const char* kTextDisabledHex = "#a8a8a8";
inline constexpr const char* kTextPrimaryHex = "#ffffff";
inline constexpr const char* kTextSecondaryHex = "#e8e8e8";

// ----- Color QColor accessors (inline; QColor not constexpr) ------

inline QColor accent() { return QColor(QString::fromLatin1("#00e5ff")); }
inline QColor bgElevated() { return QColor(QString::fromLatin1("#101010")); }
inline QColor bgPrimary() { return QColor(QString::fromLatin1("#000000")); }
inline QColor bgSurface() { return QColor(QString::fromLatin1("#050505")); }
inline QColor border() { return QColor(QString::fromLatin1("#ffffff")); }
inline QColor borderFocus() { return QColor(QString::fromLatin1("#00e5ff")); }
inline QColor modeEnded() { return QColor(QString::fromLatin1("#ffffff")); }
inline QColor modeLive() { return QColor(QString::fromLatin1("#00ff66")); }
inline QColor modePaused() { return QColor(QString::fromLatin1("#ffff00")); }
inline QColor modeRecording() { return QColor(QString::fromLatin1("#ff4040")); }
inline QColor modeReplay() { return QColor(QString::fromLatin1("#00e5ff")); }
inline QColor severityError() { return QColor(QString::fromLatin1("#ff4040")); }
inline QColor severityInfo() { return QColor(QString::fromLatin1("#ffffff")); }
inline QColor severityWarning() { return QColor(QString::fromLatin1("#ffff00")); }
inline QColor signal0() { return QColor(QString::fromLatin1("#00e5ff")); }
inline QColor signal1() { return QColor(QString::fromLatin1("#ff4040")); }
inline QColor signal2() { return QColor(QString::fromLatin1("#00ff66")); }
inline QColor signal3() { return QColor(QString::fromLatin1("#ffff00")); }
inline QColor signal4() { return QColor(QString::fromLatin1("#ff66ff")); }
inline QColor signal5() { return QColor(QString::fromLatin1("#66ffff")); }
inline QColor signal6() { return QColor(QString::fromLatin1("#ff9f00")); }
inline QColor signal7() { return QColor(QString::fromLatin1("#ffffff")); }
inline QColor statusConnected() { return QColor(QString::fromLatin1("#00ff66")); }
inline QColor statusConnecting() { return QColor(QString::fromLatin1("#ffff00")); }
inline QColor statusDisconnecting() { return QColor(QString::fromLatin1("#ffff00")); }
inline QColor statusError() { return QColor(QString::fromLatin1("#ff4040")); }
inline QColor statusIdle() { return QColor(QString::fromLatin1("#ffffff")); }
inline QColor textDisabled() { return QColor(QString::fromLatin1("#a8a8a8")); }
inline QColor textPrimary() { return QColor(QString::fromLatin1("#ffffff")); }
inline QColor textSecondary() { return QColor(QString::fromLatin1("#e8e8e8")); }

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

}  // namespace signalforge::tokens::high_contrast
