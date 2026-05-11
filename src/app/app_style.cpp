// M16 S4 — SignalForge visual identity ownership.
//
// See `app_style.hpp` for design + sequencing rationale. This
// file implements the rendering-contract enforcement per
// `docs/v0.3/rendering-environment-lock.md`.

#include "app_style.hpp"

#include "generated_style_tokens.hpp"
#include "observability/logging.hpp"

#include <QApplication>
#include <QByteArray>
#include <QFile>
#include <QFont>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QPalette>
#include <QScreen>
#include <QString>
#include <QStringList>
#include <QStyle>
#include <QStyleFactory>

namespace signalforge::app {

namespace {

// Active theme storage. M16: always Light; M20 may flip.
SignalForgeStyle::Theme g_active_theme = SignalForgeStyle::Theme::Light;

// Internal flag set by applyAtStartup once Fusion + fonts + palette
// + QSS have been applied. verifyEnvironmentContract reads this in
// preference to QApplication::style()->objectName() because
// QApplication::setStyleSheet() wraps the active style in
// QStyleSheetStyle (an internal QProxyStyle); the wrap obscures the
// objectName tag we'd otherwise set on the Fusion instance.
bool g_contract_applied = false;

// Convert a hex string from `generated_style_tokens.hpp` (e.g.
// "#fbfbfa") into a QColor at runtime. `QColor` is not
// constexpr in Qt 6.10, so this lives at function scope.
QColor hexColor(const char* hex) {
    return QColor(QString::fromLatin1(hex));
}

// Bundled font load helper. Returns true if the resource path
// was registered with Qt's font database, false otherwise.
// Logs the family / failure to the SF observability logger.
bool addFont(const QString& resourcePath, bool fail_fast_on_error) {
    const int id = QFontDatabase::addApplicationFont(resourcePath);
    if (id < 0) {
        SF_LOG_ERROR("SignalForgeStyle: QFontDatabase::addApplicationFont failed for '{}'", resourcePath.toStdString());
        if (fail_fast_on_error) {
            qFatal("SignalForgeStyle: required font '%s' failed to load — M16 rendering contract violated",
                   qUtf8Printable(resourcePath));
        }
        return false;
    }
    const QStringList families = QFontDatabase::applicationFontFamilies(id);
    if (families.isEmpty()) {
        SF_LOG_WARN("SignalForgeStyle: font '{}' loaded but registered 0 families", resourcePath.toStdString());
    } else {
        SF_LOG_INFO("SignalForgeStyle: loaded '{}' (family='{}')", resourcePath.toStdString(),
                    families.first().toStdString());
    }
    return true;
}

}  // namespace

void SignalForgeStyle::applyAtStartup(QApplication* app) {
    if (app == nullptr) {
        qFatal("SignalForgeStyle::applyAtStartup: QApplication is null");
    }

    // ── 1. Force Fusion style — pre-fonts, pre-palette ──────────────────
    QStyle* fusion = QStyleFactory::create(QStringLiteral("Fusion"));
    if (fusion == nullptr) {
        qFatal("SignalForgeStyle::applyAtStartup: Fusion QStyle unavailable — Qt build missing Fusion?");
    }
    QApplication::setStyle(fusion);

    // ── 2. Load bundled fonts (fail-fast on Inter Regular) ──────────────
    if (!loadBundledFonts()) {
        qFatal("SignalForgeStyle::applyAtStartup: bundled fonts failed to load — M16 rendering contract violated");
    }

    // ── 3. Apply 18-ColorRole light palette ─────────────────────────────
    applyLightPalette(app);

    // ── 4. Apply :/styles/tokens.qss stylesheet ─────────────────────────
    applyGlobalStylesheet(app);

    // ── 5. Default app font: Inter @ tokens body size ───────────────────
    QApplication::setFont(QFont(QString::fromLatin1(tokens::light::kFontFamilySans), tokens::light::kFontSizeBody));

    g_contract_applied = true;

    // ── 6. Optional runtime contract verification ───────────────────────
    if (qEnvironmentVariableIsSet("SF_VERIFY_RENDER_ENV")) {
        if (!verifyEnvironmentContract()) {
            qFatal("SignalForgeStyle::applyAtStartup: environment contract verification failed");
        }
        SF_LOG_INFO("SignalForgeStyle: SF_VERIFY_RENDER_ENV check PASS");
    }

    SF_LOG_INFO("SignalForgeStyle: applied (Fusion + Inter@{}pt + tokens.qss)", tokens::light::kFontSizeBody);
}

SignalForgeStyle::Theme SignalForgeStyle::activeTheme() noexcept {
    return g_active_theme;
}

void SignalForgeStyle::setActiveTheme(Theme t) {
    if (t == Theme::Dark) {
        SF_LOG_WARN("SignalForgeStyle::setActiveTheme(Dark): not implemented at M16 — remaining on Light (M20 slot)");
        g_active_theme = Theme::Light;
        return;
    }
    g_active_theme = t;
}

bool SignalForgeStyle::verifyEnvironmentContract() {
    // Tier 2 — Qt rendering stack. Check the internal flag set
    // by applyAtStartup rather than QApplication::style()->objectName()
    // because Qt wraps the active style in QStyleSheetStyle when
    // setStyleSheet() is called, obscuring the original Fusion
    // instance's objectName. The flag confirms SignalForgeStyle
    // applied the contract; for a deeper QStyle assertion the
    // S6 cross-env pixel-diff (compare_with_contract) is the
    // authoritative gate.
    if (!g_contract_applied) {
        SF_LOG_ERROR("SignalForgeStyle::verifyEnvironmentContract: applyAtStartup not invoked");
        return false;
    }

    // Tier 1 — Font cascade. Verify app default family matches
    // the M16 expected sans family. Qt's family resolution may
    // pick a closest match if Inter weren't loaded; comparing
    // the QApplication::font() family name is the cheapest
    // honest check.
    const QString default_family = QApplication::font().family();
    const QString expected_sans = QString::fromLatin1(tokens::light::kFontFamilySans);
    if (default_family != expected_sans) {
        SF_LOG_ERROR("SignalForgeStyle::verifyEnvironmentContract: default font family is '{}', expected '{}'",
                     default_family.toStdString(), expected_sans.toStdString());
        return false;
    }

    // Tier 3 — Display geometry. DPR forced 1.0 per contract.
    if (QGuiApplication::primaryScreen() != nullptr) {
        const qreal dpr = QGuiApplication::primaryScreen()->devicePixelRatio();
        if (dpr < 0.99 || dpr > 1.01) {
            SF_LOG_ERROR("SignalForgeStyle::verifyEnvironmentContract: DPR is {}, expected 1.0",
                         static_cast<double>(dpr));
            return false;
        }
    }

    return true;
}

bool SignalForgeStyle::loadBundledFonts() {
    // Inter Regular — fail-fast. The S0.5 spike empirical
    // foundation depends on this exact OTF (sha256 in env contract).
    const bool inter_regular = addFont(QStringLiteral(":/fonts/Inter-Regular.otf"),
                                       /*fail_fast_on_error=*/true);
    if (!inter_regular) {
        return false;
    }

    // Inter Medium / Bold / Italic — advisory (M17+ widget rebuild
    // consumes these for emphasis / headings / italic accent).
    (void)addFont(QStringLiteral(":/fonts/Inter-Medium.otf"), /*fail_fast_on_error=*/false);
    (void)addFont(QStringLiteral(":/fonts/Inter-Bold.otf"), /*fail_fast_on_error=*/false);
    (void)addFont(QStringLiteral(":/fonts/Inter-Italic.otf"), /*fail_fast_on_error=*/false);

    // JetBrains Mono Regular / Medium — required for measurement
    // readouts per manifesto §2.2. Regular fail-fast; Medium
    // advisory.
    const bool jbm_regular = addFont(QStringLiteral(":/fonts/JetBrainsMono-Regular.ttf"),
                                     /*fail_fast_on_error=*/true);
    if (!jbm_regular) {
        return false;
    }
    (void)addFont(QStringLiteral(":/fonts/JetBrainsMono-Medium.ttf"), /*fail_fast_on_error=*/false);

    return true;
}

void SignalForgeStyle::applyLightPalette(QApplication* app) {
    QPalette p;

    // 18 standard QPalette::ColorRole values mapped from
    // generated_style_tokens.hpp. Sources per M16-concerns §C5
    // mapping table.

    // Top-level surfaces
    p.setColor(QPalette::Window, hexColor(tokens::light::kBgPrimaryHex));
    p.setColor(QPalette::WindowText, hexColor(tokens::light::kTextPrimaryHex));
    p.setColor(QPalette::Base, hexColor(tokens::light::kBgSurfaceHex));
    p.setColor(QPalette::AlternateBase, hexColor(tokens::light::kBgElevatedHex));

    // Tooltips
    p.setColor(QPalette::ToolTipBase, hexColor(tokens::light::kBgElevatedHex));
    p.setColor(QPalette::ToolTipText, hexColor(tokens::light::kTextPrimaryHex));

    // Text & controls
    p.setColor(QPalette::Text, hexColor(tokens::light::kTextPrimaryHex));
    p.setColor(QPalette::Button, hexColor(tokens::light::kBgElevatedHex));
    p.setColor(QPalette::ButtonText, hexColor(tokens::light::kTextPrimaryHex));
    p.setColor(QPalette::BrightText, hexColor(tokens::light::kTextPrimaryHex));

    // Selection / focus
    p.setColor(QPalette::Highlight, hexColor(tokens::light::kBorderFocusHex));
    p.setColor(QPalette::HighlightedText, hexColor(tokens::light::kBgPrimaryHex));

    // Links (rarely used; mapped sensibly anyway)
    p.setColor(QPalette::Link, hexColor(tokens::light::kBorderFocusHex));
    p.setColor(QPalette::LinkVisited, hexColor(tokens::light::kSignal4Hex));  // purple

    // Frame / shadow shades (Fusion uses these for borders + gradients)
    p.setColor(QPalette::Light, hexColor(tokens::light::kBgSurfaceHex));
    p.setColor(QPalette::Midlight, hexColor(tokens::light::kBgElevatedHex));
    p.setColor(QPalette::Dark, hexColor(tokens::light::kTextSecondaryHex));
    p.setColor(QPalette::Mid, hexColor(tokens::light::kBorderHex));
    p.setColor(QPalette::Shadow, hexColor(tokens::light::kTextPrimaryHex));

    // PlaceholderText (Qt 5.12+; QLineEdit / QTextEdit placeholder)
    p.setColor(QPalette::PlaceholderText, hexColor(tokens::light::kTextDisabledHex));

    // Disabled ColorGroup — override Text / ButtonText / WindowText
    // with the disabled token so visually-disabled controls demote.
    const QColor disabled_text = hexColor(tokens::light::kTextDisabledHex);
    p.setColor(QPalette::Disabled, QPalette::WindowText, disabled_text);
    p.setColor(QPalette::Disabled, QPalette::Text, disabled_text);
    p.setColor(QPalette::Disabled, QPalette::ButtonText, disabled_text);

    QApplication::setPalette(p);
}

void SignalForgeStyle::applyGlobalStylesheet(QApplication* app) {
    if (app == nullptr) {
        return;
    }
    QFile qss(QStringLiteral(":/styles/tokens.qss"));
    if (!qss.open(QIODevice::ReadOnly | QIODevice::Text)) {
        SF_LOG_WARN("SignalForgeStyle::applyGlobalStylesheet: :/styles/tokens.qss not found in resources");
        return;
    }
    const QByteArray content = qss.readAll();
    qss.close();
    app->setStyleSheet(QString::fromUtf8(content));
    SF_LOG_INFO("SignalForgeStyle: applied stylesheet ({} bytes)", content.size());
}

}  // namespace signalforge::app
