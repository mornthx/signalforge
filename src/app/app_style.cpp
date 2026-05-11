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
    tier1.insert(QStringLiteral("app_default_family"), QString::fromLatin1(tokens::light::kFontFamilySans));
    tier1.insert(QStringLiteral("app_default_size_pt"), tokens::light::kFontSizeBody);
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
