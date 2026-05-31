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
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QPalette>
#include <QProcessEnvironment>
#include <QScreen>
#include <QString>
#include <QStringList>
#include <QStyle>
#include <QStyleFactory>
#include <QSysInfo>
#include <QtVersion>

namespace signalforge::app {

namespace {

SignalForgeStyle::Theme g_active_theme = SignalForgeStyle::Theme::Dark;

struct ThemeTokens {
    const char* bgPrimary;
    const char* bgSurface;
    const char* bgElevated;
    const char* border;
    const char* borderFocus;
    const char* textPrimary;
    const char* textSecondary;
    const char* textDisabled;
    const char* signal4;
    const char* fontSans;
    int fontBody;
    const char* qssResource;
};

ThemeTokens tokensForTheme(SignalForgeStyle::Theme theme) {
    switch (theme) {
    case SignalForgeStyle::Theme::Dark:
        return {tokens::dark::kBgPrimaryHex,     tokens::dark::kBgSurfaceHex,
                tokens::dark::kBgElevatedHex,    tokens::dark::kBorderHex,
                tokens::dark::kBorderFocusHex,   tokens::dark::kTextPrimaryHex,
                tokens::dark::kTextSecondaryHex, tokens::dark::kTextDisabledHex,
                tokens::dark::kSignal4Hex,       tokens::dark::kFontFamilySans,
                tokens::dark::kFontSizeBody,     ":/styles/tokens-dark.qss"};
    case SignalForgeStyle::Theme::HighContrast:
        return {tokens::high_contrast::kBgPrimaryHex,     tokens::high_contrast::kBgSurfaceHex,
                tokens::high_contrast::kBgElevatedHex,    tokens::high_contrast::kBorderHex,
                tokens::high_contrast::kBorderFocusHex,   tokens::high_contrast::kTextPrimaryHex,
                tokens::high_contrast::kTextSecondaryHex, tokens::high_contrast::kTextDisabledHex,
                tokens::high_contrast::kSignal4Hex,       tokens::high_contrast::kFontFamilySans,
                tokens::high_contrast::kFontSizeBody,     ":/styles/tokens-high-contrast.qss"};
    case SignalForgeStyle::Theme::Light:
        return {tokens::light::kBgPrimaryHex,     tokens::light::kBgSurfaceHex,
                tokens::light::kBgElevatedHex,    tokens::light::kBorderHex,
                tokens::light::kBorderFocusHex,   tokens::light::kTextPrimaryHex,
                tokens::light::kTextSecondaryHex, tokens::light::kTextDisabledHex,
                tokens::light::kSignal4Hex,       tokens::light::kFontFamilySans,
                tokens::light::kFontSizeBody,     ":/styles/tokens.qss"};
    }
    return tokensForTheme(SignalForgeStyle::Theme::Light);
}

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

    // ── 3. Apply 18-ColorRole palette for the active theme ──────────────
    applyPalette(app, g_active_theme);

    // ── 4. Apply :/styles/tokens.qss stylesheet ─────────────────────────
    applyGlobalStylesheet(app);

    // ── 5. Default app font: Inter @ tokens body size ───────────────────
    const ThemeTokens tt = tokensForTheme(g_active_theme);
    QApplication::setFont(QFont(QString::fromLatin1(tt.fontSans), tt.fontBody));

    g_contract_applied = true;

    // ── 6. Optional runtime contract verification ───────────────────────
    if (qEnvironmentVariableIsSet("SF_VERIFY_RENDER_ENV")) {
        if (!verifyEnvironmentContract()) {
            qFatal("SignalForgeStyle::applyAtStartup: environment contract verification failed");
        }
        SF_LOG_INFO("SignalForgeStyle: SF_VERIFY_RENDER_ENV check PASS");
    }

    SF_LOG_INFO("SignalForgeStyle: applied (Fusion + Inter@{}pt + theme={})", tt.fontBody,
                themeName(g_active_theme).toStdString());
}

SignalForgeStyle::Theme SignalForgeStyle::activeTheme() noexcept {
    return g_active_theme;
}

void SignalForgeStyle::setActiveTheme(Theme t) {
    auto* app = qobject_cast<QApplication*>(QApplication::instance());
    g_active_theme = t;
    if (app == nullptr) {
        return;
    }
    applyPalette(app, t);
    applyGlobalStylesheet(app);
    const ThemeTokens tt = tokensForTheme(t);
    QApplication::setFont(QFont(QString::fromLatin1(tt.fontSans), tt.fontBody));
    SF_LOG_INFO("SignalForgeStyle: theme switched to '{}'", themeName(t).toStdString());
}

QString SignalForgeStyle::themeName(Theme t) {
    switch (t) {
    case Theme::Light:
        return QStringLiteral("light");
    case Theme::Dark:
        return QStringLiteral("dark");
    case Theme::HighContrast:
        return QStringLiteral("high_contrast");
    }
    return QStringLiteral("light");
}

SignalForgeStyle::Theme SignalForgeStyle::themeFromName(const QString& name, bool* ok) {
    const QString normalized = name.trimmed().toLower().replace(QLatin1Char('-'), QLatin1Char('_'));
    if (ok != nullptr) {
        *ok = true;
    }
    if (normalized == QStringLiteral("light")) {
        return Theme::Light;
    }
    if (normalized == QStringLiteral("dark")) {
        return Theme::Dark;
    }
    if (normalized == QStringLiteral("high_contrast") || normalized == QStringLiteral("contrast") ||
        normalized == QStringLiteral("highcontrast")) {
        return Theme::HighContrast;
    }
    if (ok != nullptr) {
        *ok = false;
    }
    return Theme::Light;
}

bool SignalForgeStyle::verifyEnvironmentContract() {
    if (!g_contract_applied) {
        SF_LOG_ERROR("SignalForgeStyle::verifyEnvironmentContract: applyAtStartup not invoked");
        return false;
    }

    const QString default_family = QApplication::font().family();
    const ThemeTokens tt = tokensForTheme(g_active_theme);
    const QString expected_sans = QString::fromLatin1(tt.fontSans);
    if (default_family != expected_sans) {
        SF_LOG_ERROR("SignalForgeStyle::verifyEnvironmentContract: default font family is '{}', expected '{}'",
                     default_family.toStdString(), expected_sans.toStdString());
        return false;
    }

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
    const bool inter_regular = addFont(QStringLiteral(":/fonts/Inter-Regular.otf"),
                                       /*fail_fast_on_error=*/true);
    if (!inter_regular) {
        return false;
    }

    (void)addFont(QStringLiteral(":/fonts/Inter-Medium.otf"), /*fail_fast_on_error=*/false);
    (void)addFont(QStringLiteral(":/fonts/Inter-Bold.otf"), /*fail_fast_on_error=*/false);
    (void)addFont(QStringLiteral(":/fonts/Inter-Italic.otf"), /*fail_fast_on_error=*/false);

    const bool jbm_regular = addFont(QStringLiteral(":/fonts/JetBrainsMono-Regular.ttf"),
                                     /*fail_fast_on_error=*/true);
    if (!jbm_regular) {
        return false;
    }
    (void)addFont(QStringLiteral(":/fonts/JetBrainsMono-Medium.ttf"), /*fail_fast_on_error=*/false);

    return true;
}

void SignalForgeStyle::applyPalette(QApplication* app, Theme theme) {
    if (app == nullptr) {
        return;
    }
    QPalette p;
    const ThemeTokens tt = tokensForTheme(theme);

    // 18 standard QPalette::ColorRole values mapped from
    // generated_style_tokens.hpp. Sources per M16-concerns §C5
    // mapping table.

    // Top-level surfaces
    p.setColor(QPalette::Window, hexColor(tt.bgPrimary));
    p.setColor(QPalette::WindowText, hexColor(tt.textPrimary));
    p.setColor(QPalette::Base, hexColor(tt.bgSurface));
    p.setColor(QPalette::AlternateBase, hexColor(tt.bgElevated));

    // Tooltips
    p.setColor(QPalette::ToolTipBase, hexColor(tt.bgElevated));
    p.setColor(QPalette::ToolTipText, hexColor(tt.textPrimary));

    // Text & controls
    p.setColor(QPalette::Text, hexColor(tt.textPrimary));
    p.setColor(QPalette::Button, hexColor(tt.bgElevated));
    p.setColor(QPalette::ButtonText, hexColor(tt.textPrimary));
    p.setColor(QPalette::BrightText, hexColor(tt.textPrimary));

    // Selection / focus
    p.setColor(QPalette::Highlight, hexColor(tt.borderFocus));
    p.setColor(QPalette::HighlightedText, hexColor(tt.bgPrimary));

    // Links (rarely used; mapped sensibly anyway)
    p.setColor(QPalette::Link, hexColor(tt.borderFocus));
    p.setColor(QPalette::LinkVisited, hexColor(tt.signal4));

    // Frame / shadow shades (Fusion uses these for borders + gradients)
    p.setColor(QPalette::Light, hexColor(tt.bgSurface));
    p.setColor(QPalette::Midlight, hexColor(tt.bgElevated));
    p.setColor(QPalette::Dark, hexColor(tt.textSecondary));
    p.setColor(QPalette::Mid, hexColor(tt.border));
    p.setColor(QPalette::Shadow, hexColor(tt.textPrimary));

    // PlaceholderText (Qt 5.12+; QLineEdit / QTextEdit placeholder)
    p.setColor(QPalette::PlaceholderText, hexColor(tt.textDisabled));

    // Disabled ColorGroup — override Text / ButtonText / WindowText
    // with the disabled token so visually-disabled controls demote.
    const QColor disabled_text = hexColor(tt.textDisabled);
    p.setColor(QPalette::Disabled, QPalette::WindowText, disabled_text);
    p.setColor(QPalette::Disabled, QPalette::Text, disabled_text);
    p.setColor(QPalette::Disabled, QPalette::ButtonText, disabled_text);

    QApplication::setPalette(p);
}

void SignalForgeStyle::applyLightPalette(QApplication* app) {
    applyPalette(app, Theme::Light);
}

void SignalForgeStyle::applyGlobalStylesheet(QApplication* app) {
    if (app == nullptr) {
        return;
    }
    const ThemeTokens tt = tokensForTheme(g_active_theme);
    QFile qss(QString::fromLatin1(tt.qssResource));
    if (!qss.open(QIODevice::ReadOnly | QIODevice::Text)) {
        SF_LOG_WARN("SignalForgeStyle::applyGlobalStylesheet: '{}' not found in resources", tt.qssResource);
        return;
    }
    const QByteArray content = qss.readAll();
    qss.close();
    app->setStyleSheet(QString::fromUtf8(content));
    SF_LOG_INFO("SignalForgeStyle: applied stylesheet '{}' ({} bytes)", tt.qssResource, content.size());
}

namespace {

// Major.minor extracted from Qt's QT_VERSION_STR macro (e.g. "6.10.2" → "6.10").
QString qtVersionMajorMinor() {
    const QString full = QString::fromLatin1(QT_VERSION_STR);
    const int second_dot = full.indexOf('.', full.indexOf('.') + 1);
    return second_dot > 0 ? full.left(second_dot) : full;
}

// Format primary-screen geometry as "WxH" string (DPR=1.0 expected).
QString screenGeometry() {
    if (QGuiApplication::primaryScreen() == nullptr) {
        return QString();
    }
    const auto sz = QGuiApplication::primaryScreen()->size();
    return QStringLiteral("%1x%2").arg(sz.width()).arg(sz.height());
}

// Read disallowed env vars (per rendering-environment-lock.md §3.2)
// + emit a JSON array of any that are SET at capture time. Empty
// array = clean env per contract.
QJsonArray disallowedEnvOverridesPresent() {
    const QStringList disallowed = {
        QStringLiteral("QT_SCALE_FACTOR"),           QStringLiteral("QT_AUTO_SCREEN_SCALE_FACTOR"),
        QStringLiteral("QT_ENABLE_HIGHDPI_SCALING"), QStringLiteral("QT_FONT_DPI"),
        QStringLiteral("QT_STYLE_OVERRIDE"),
    };
    QJsonArray present;
    const QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    for (const QString& key : disallowed) {
        if (!env.contains(key)) {
            continue;
        }
        const QString val = env.value(key);
        // QT_ENABLE_HIGHDPI_SCALING = "0" or unset is OK; flag only when set non-zero
        if (key == QStringLiteral("QT_ENABLE_HIGHDPI_SCALING") && val == QStringLiteral("0")) {
            continue;
        }
        // QT_STYLE_OVERRIDE: signed-off only when SignalForgeStyle is the setter;
        // external presence at runtime is flagged but the field exists for traceability.
        present.append(QStringLiteral("%1=%2").arg(key, val));
    }
    return present;
}

// Tier 4 advisory observability — record but never gate.
QJsonObject tier4Advisory(const QProcessEnvironment& env) {
    QJsonObject t4;
    t4.insert(QStringLiteral("os"), QSysInfo::prettyProductName());
    t4.insert(QStringLiteral("kernel"), QSysInfo::kernelVersion());
    t4.insert(QStringLiteral("kernel_type"), QSysInfo::kernelType());
    t4.insert(QStringLiteral("machine_arch"), QSysInfo::currentCpuArchitecture());
    t4.insert(QStringLiteral("xdg_current_desktop"), env.value(QStringLiteral("XDG_CURRENT_DESKTOP")));
    t4.insert(QStringLiteral("desktop_session"), env.value(QStringLiteral("DESKTOP_SESSION")));
    t4.insert(QStringLiteral("gtk_theme_env"), env.value(QStringLiteral("GTK_THEME")));
    t4.insert(QStringLiteral("qt_im_module_env"), env.value(QStringLiteral("QT_IM_MODULE")));
    t4.insert(QStringLiteral("qt_accessibility_env"), env.value(QStringLiteral("QT_ACCESSIBILITY")));
    t4.insert(QStringLiteral("qt_plugin_path_env"), env.value(QStringLiteral("QT_PLUGIN_PATH")));
    t4.insert(QStringLiteral("qt_root_dir_env"), env.value(QStringLiteral("QT_ROOT_DIR")));
    t4.insert(QStringLiteral("qsg_rhi_backend_env"), env.value(QStringLiteral("QSG_RHI_BACKEND")));
    return t4;
}

}  // namespace

bool SignalForgeStyle::dumpEnvironmentJson(const QString& outPath) {
    if (outPath.isEmpty()) {
        SF_LOG_WARN("SignalForgeStyle::dumpEnvironmentJson: empty path");
        return false;
    }
    const QProcessEnvironment env = QProcessEnvironment::systemEnvironment();

    // ── Tier 1 — Font cascade (the determinism keystone per S0.5) ────────
    QJsonObject tier1;
    const ThemeTokens tt = tokensForTheme(g_active_theme);
    tier1.insert(QStringLiteral("app_default_family"), QString::fromLatin1(tt.fontSans));
    tier1.insert(QStringLiteral("app_default_size_pt"), tt.fontBody);
    tier1.insert(QStringLiteral("app_mono_family"), QString::fromLatin1(tokens::light::kFontFamilyMono));
    // Observed (post-applyAtStartup) — verify the running QApplication font matches.
    tier1.insert(QStringLiteral("observed_default_family"), QApplication::font().family());
    tier1.insert(QStringLiteral("observed_default_size_pt"), QApplication::font().pointSize());

    // ── Tier 2 — Qt rendering stack ──────────────────────────────────────
    QJsonObject tier2;
    tier2.insert(QStringLiteral("qt_version_major_minor"), qtVersionMajorMinor());
    tier2.insert(QStringLiteral("qt_version_full"), QString::fromLatin1(QT_VERSION_STR));
    tier2.insert(QStringLiteral("qpa_platform"), QGuiApplication::platformName());
    // style_object_introspection: record "Fusion" as the SignalForgeStyle-
    // enforced value, NOT QApplication::style()->objectName() (which is
    // empty after setStyleSheet wraps Fusion in QStyleSheetStyle, per
    // S4 Phase-4 operator caveat).
    tier2.insert(QStringLiteral("style_object_introspection"), QStringLiteral("Fusion"));
    tier2.insert(QStringLiteral("style_recording_note"),
                 QStringLiteral("set-as-applied by SignalForgeStyle::applyAtStartup; "
                                "not post-QSS-wrap introspected (Qt's QStyleSheetStyle "
                                "wraps QFusionStyle when setStyleSheet runs, hiding "
                                "objectName)."));
    tier2.insert(QStringLiteral("wayland_disallowed"),
                 !QGuiApplication::platformName().contains(QStringLiteral("wayland"), Qt::CaseInsensitive));
    // gpu_rasterization_disallowed: SignalForgeStyle does not enforce
    // QSG_RHI_BACKEND directly; per ADR-010 the capture script sets it.
    // We record whether QSG_RHI_BACKEND is `software` or unset (both OK
    // for the contract); only explicit non-software values fail.
    const QString rhi = env.value(QStringLiteral("QSG_RHI_BACKEND"));
    tier2.insert(QStringLiteral("gpu_rasterization_disallowed"),
                 rhi.isEmpty() || rhi.compare(QStringLiteral("software"), Qt::CaseInsensitive) == 0);
    tier2.insert(QStringLiteral("qt_env_overrides_present"), disallowedEnvOverridesPresent());

    // ── Tier 3 — Display geometry ────────────────────────────────────────
    QJsonObject tier3;
    const qreal dpr =
        QGuiApplication::primaryScreen() != nullptr ? QGuiApplication::primaryScreen()->devicePixelRatio() : 1.0;
    // Store as a string with 1-decimal precision so JSON serialisation
    // is stable across hosts (Qt's QJsonValue may emit `1` vs `1.0`
    // depending on the underlying integral-vs-float representation;
    // string form sidesteps that drift for the contract pre-check).
    tier3.insert(QStringLiteral("device_pixel_ratio"), QString::number(dpr, 'f', 1));
    tier3.insert(QStringLiteral("logical_dpi"),
                 QString::number(QGuiApplication::primaryScreen() != nullptr
                                     ? QGuiApplication::primaryScreen()->logicalDotsPerInch()
                                     : 96.0,
                                 'f', 0));
    tier3.insert(QStringLiteral("physical_dpi_observed"),
                 QString::number(QGuiApplication::primaryScreen() != nullptr
                                     ? QGuiApplication::primaryScreen()->physicalDotsPerInch()
                                     : 96.0,
                                 'f', 1));
    tier3.insert(QStringLiteral("screen_geometry"), screenGeometry());
    tier3.insert(QStringLiteral("xvfb_screen_depth"),
                 QGuiApplication::primaryScreen() != nullptr ? QGuiApplication::primaryScreen()->depth() : 24);
    tier3.insert(QStringLiteral("locale"), QLocale().name());

    // ── Top-level sidecar shape ──────────────────────────────────────────
    QJsonObject root;
    root.insert(QStringLiteral("spike"), QStringLiteral(""));  // M16 S5: no longer spike runs
    root.insert(QStringLiteral("captured_by"), QStringLiteral("SignalForgeStyle::dumpEnvironmentJson"));
    root.insert(QStringLiteral("theme"), themeName(g_active_theme));
    root.insert(QStringLiteral("contract_version"),
                QStringLiteral("M16 S5; matches tests/visual/lib/compare.py ENV_CONTRACT_REQUIRED_KEYS"));
    root.insert(QStringLiteral("tier_1_font_cascade"), tier1);
    root.insert(QStringLiteral("tier_2_qt_rendering"), tier2);
    root.insert(QStringLiteral("tier_3_geometry"), tier3);
    root.insert(QStringLiteral("tier_4_advisory"), tier4Advisory(env));

    const QByteArray json = QJsonDocument(root).toJson(QJsonDocument::Indented);
    QFile out(outPath);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        SF_LOG_ERROR("SignalForgeStyle::dumpEnvironmentJson: cannot open '{}' for write", outPath.toStdString());
        return false;
    }
    out.write(json);
    out.close();
    SF_LOG_INFO("SignalForgeStyle::dumpEnvironmentJson: wrote {} bytes to '{}'", json.size(), outPath.toStdString());
    return true;
}

}  // namespace signalforge::app
