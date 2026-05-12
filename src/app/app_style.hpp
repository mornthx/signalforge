#pragma once

// M16 S4 — SignalForge visual identity ownership entry point.
//
// `SignalForgeStyle::applyAtStartup` is called from `main.cpp`
// after `QApplication` is constructed and BEFORE `MainWindow`
// is shown. It establishes the M16 rendering contract per
// `docs/v0.3/rendering-environment-lock.md`:
//
//   1. Force Fusion style (`QApplication::setStyle`).
//   2. Load bundled fonts from `:/fonts/` (Qt resource compile;
//      lockstep with binary).
//   3. Build explicit 18-ColorRole `QPalette` from
//      `generated_style_tokens.hpp` + apply via
//      `QApplication::setPalette`.
//   4. Load `tokens.qss` from `:/styles/` + apply via
//      `QApplication::setStyleSheet`.
//   5. Set default app font (`Inter`, 12 pt).
//
// Fail-fast on Inter Regular load failure (`qFatal` — M16 stack
// is broken without it; better to crash early than render with
// the wrong font cascade).
//
// Continuity with M16 S0.5 R13 spike (per
// `docs/v0.3/spike-result.md`): the bundled Inter Regular OTF
// is byte-identical to the spike artifact (sha256 recorded in
// `rendering-environment-lock.md` §2.1). The spike empirically
// validated this stack reduces cross-environment diff to
// 0.12–0.30 %; S4 extends from the spike's 6-role minimal
// palette to the full 18-role `QPalette` + global `QSS` without
// regression.

#include <QColor>
#include <QString>

class QApplication;

namespace signalforge::app {

class SignalForgeStyle {
public:
    /// Theme variants. M16 ships `Light` only; `Dark` slot
    /// defined for M20 dark-theme implementation. Calling
    /// `setActiveTheme(Dark)` at M16 logs a `qWarning` +
    /// falls back to `Light` (per `M16-concerns.md` §C9).
    enum class Theme {
        Light,
        Dark,  // M20 slot — M16 falls back to Light with warning
    };

    /// Establish the M16 rendering contract on `app`. Must be
    /// called BEFORE constructing widgets (so the palette /
    /// stylesheet / default font are picked up at widget
    /// construction time).
    ///
    /// Effect order:
    ///   1. setStyle(Fusion)         — pre-fonts, pre-palette
    ///   2. loadBundledFonts()       — Inter + JetBrains Mono
    ///                                 from :/fonts/
    ///   3. applyLightPalette()      — explicit 18-ColorRole set
    ///   4. applyGlobalStylesheet()  — tokens.qss from :/styles/
    ///   5. setFont(Inter, 12)       — app default text
    ///
    /// Fail-fast on Inter Regular load failure (`qFatal`).
    /// Returns normally otherwise.
    ///
    /// If the env var `SF_VERIFY_RENDER_ENV` is set, also
    /// invokes `verifyEnvironmentContract()` at the end and
    /// `qFatal`s on mismatch.
    static void applyAtStartup(QApplication* app);

    /// Currently-active theme. M16 ships Light only.
    [[nodiscard]] static Theme activeTheme() noexcept;

    /// M16: only Light supported. Calling with Dark logs
    /// warning + remains on Light (Dark implementation at M20).
    /// Per `docs/v0.3/visual-identity.md` §4 theme context.
    static void setActiveTheme(Theme t);

    /// Runtime introspection of the M16 rendering contract.
    /// Reads back `QApplication::style()->objectName()`,
    /// `QApplication::font()`, primary screen DPR, etc., and
    /// compares against expected M16 values. Returns true if
    /// every required field matches; false otherwise (caller
    /// `qFatal`s in the strict path).
    ///
    /// Invoked at startup when `SF_VERIFY_RENDER_ENV` env var
    /// is set (typically by CI smoke tests + the per-capture
    /// env-dump script at M16 S5).
    [[nodiscard]] static bool verifyEnvironmentContract();

    /// Internal — exposed for the unit-test path. Loads Inter
    /// Regular / Medium / Bold / Italic + JetBrains Mono
    /// Regular / Medium via `QFontDatabase::addApplicationFont`
    /// from `:/fonts/*.otf|ttf`. Returns true if Inter Regular
    /// loaded; partial-success on the other variants is
    /// tolerated (advisory).
    [[nodiscard]] static bool loadBundledFonts();

    /// Internal — exposed for unit-test inspection. Builds the
    /// 18-ColorRole light palette from
    /// `generated_style_tokens.hpp` and applies it to `app`.
    static void applyLightPalette(QApplication* app);

    /// Internal — loads `:/styles/tokens.qss` and applies via
    /// `QApplication::setStyleSheet`.
    static void applyGlobalStylesheet(QApplication* app);

    /// M16 S5 — emit env-sidecar JSON for the current
    /// `QApplication` state per
    /// `docs/v0.3/rendering-environment-lock.md` §6. Schema is
    /// Tier 1 (font cascade) + Tier 2 (Qt rendering stack;
    /// `style_object_introspection` recorded as the
    /// SignalForgeStyle-enforced value, not post-`setStyleSheet`-
    /// wrap-introspected — per operator's S4 Phase-4 caveat) +
    /// Tier 3 (display geometry) + Tier 4 (advisory observability).
    ///
    /// Required nested keys exactly match
    /// `tests/visual/lib/compare.py:ENV_CONTRACT_REQUIRED_KEYS`
    /// (the S3 contract). Sidecar mismatch on any required field
    /// in `compare_with_contract` Step 1 → INVALID per R14 / H10.
    ///
    /// Returns true on successful write; false on I/O error.
    /// Invoked from `main.cpp`:
    ///   - automatically alongside `--capture-screenshot-path` or
    ///     `--capture-fullscreen-path` (sidecar at `<png stem>.env.json`);
    ///   - explicitly via `--dump-render-env <path>` flag for
    ///     standalone use (CI smoke + operator forensic).
    [[nodiscard]] static bool dumpEnvironmentJson(const QString& outPath);
};

}  // namespace signalforge::app
