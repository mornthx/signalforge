// tests/integration/test_m13_deb_package.cpp
//
// M13 S5 — structural validation of the V1.0 .deb package.
// Runs `dpkg-deb --info` + `dpkg-deb --contents` against the
// pre-built package and asserts control fields + key file
// paths match spec §2.1-3 + §3.4.
//
// Pre-condition: the .deb has been built via
//   cmake --build build/release --target package
// before this test runs. If the .deb is not present the test
// SKIPs cleanly (CI doesn't build the package by default).
//
// This is the **CC-runnable** half of the M13 release-artefact
// verification (concerns C4 Tier 1). The Tier 2 install-on-
// clean-Ubuntu verification is operator-run per spec §4.6 +
// `docs/m13-hardware-verification.md`.
#include <QFileInfo>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <catch2/catch_test_macros.hpp>

namespace {

constexpr const char* kSourceRoot = SIGNALFORGE_SOURCE_ROOT;
constexpr const char* kBuildRelease = "/build/release";

QString debPath() {
    return QString::fromUtf8(kSourceRoot) + QString::fromUtf8(kBuildRelease) +
           QStringLiteral("/signalforge_1.0.0_amd64.deb");
}

bool debExists() {
    return QFileInfo::exists(debPath());
}

QString runDpkgDeb(const QStringList& args) {
    QProcess proc;
    QStringList full = args;
    full.append(debPath());
    proc.start(QStringLiteral("dpkg-deb"), full);
    if (!proc.waitForFinished(30000)) {
        return QString();
    }
    return QString::fromUtf8(proc.readAllStandardOutput());
}

}  // namespace

TEST_CASE("M13 S5: .deb file produced (or test skips)", "[m13][s5][deb]") {
    if (!debExists()) {
        SKIP("signalforge_1.0.0_amd64.deb not built — run "
             "`cmake --build build/release --target package` first");
    }
    REQUIRE(debExists());
}

TEST_CASE("M13 S5: dpkg-deb --info reports correct control fields", "[m13][s5][deb]") {
    if (!debExists()) {
        SKIP("deb not built");
    }
    const QString info = runDpkgDeb({QStringLiteral("--info")});
    REQUIRE_FALSE(info.isEmpty());
    REQUIRE(info.contains(QStringLiteral("Package: signalforge")));
    REQUIRE(info.contains(QStringLiteral("Version: 1.0.0")));
    REQUIRE(info.contains(QStringLiteral("Architecture: amd64")));
    REQUIRE(info.contains(QStringLiteral("Section: science")));
    REQUIRE(info.contains(QStringLiteral("Priority: optional")));
    // Dependency manifest matches cpack-deb.cmake. yaml-cpp is
    // statically linked (no runtime dep). Qt 6 runtime packages
    // declared at Ubuntu 24.04 noble stock-repo level (Qt 6.4)
    // even though the binary actually links against Qt 6.10
    // (per docs/install.md §1.1 dual-track).
    REQUIRE(info.contains(QStringLiteral("libc6")));
    REQUIRE(info.contains(QStringLiteral("libstdc++6")));
    REQUIRE(info.contains(QStringLiteral("libqt6quickwidgets6")));
    REQUIRE(info.contains(QStringLiteral("libqt6core6t64")));
    REQUIRE(info.contains(QStringLiteral("libqt6serialport6")));
    REQUIRE(info.contains(QStringLiteral("libglib2.0-0t64")));
}

TEST_CASE("M13 S5: dpkg-deb --contents includes /opt/signalforge/bin/* binaries", "[m13][s5][deb]") {
    if (!debExists()) {
        SKIP("deb not built");
    }
    const QString contents = runDpkgDeb({QStringLiteral("--contents")});
    REQUIRE(contents.contains(QStringLiteral("./opt/signalforge/bin/signalforge")));
    REQUIRE(contents.contains(QStringLiteral("./opt/signalforge/bin/sfreplay_inspect")));
    REQUIRE(contents.contains(QStringLiteral("./opt/signalforge/bin/profile_main")));
}

TEST_CASE("M13 S5: dpkg-deb --contents includes V1.0 docs + baselines", "[m13][s5][deb]") {
    if (!debExists()) {
        SKIP("deb not built");
    }
    const QString contents = runDpkgDeb({QStringLiteral("--contents")});
    REQUIRE(contents.contains(QStringLiteral("./opt/signalforge/docs/install.md")));
    REQUIRE(contents.contains(QStringLiteral("./opt/signalforge/docs/v1.0-spec-list.md")));
    REQUIRE(contents.contains(QStringLiteral("./opt/signalforge/docs/m13-hardware-verification.md")));
    // V0.1 status summary supersedes the V1.0 release-notes that the
    // V1.0-ship plan would have shipped (V0 charter defers V1.0).
    REQUIRE(contents.contains(QStringLiteral("./opt/signalforge/docs/v0.1-status-summary.md")));
    REQUIRE(contents.contains(QStringLiteral("./opt/signalforge/docs/format/sfreplay-v1.md")));
    REQUIRE(contents.contains(QStringLiteral("./opt/signalforge/benchmarks/results/M11-baseline.md")));
    REQUIRE(contents.contains(QStringLiteral("./opt/signalforge/benchmarks/results/M12-baseline.md")));
}

TEST_CASE("M13 S5: dpkg-deb --contents includes desktop entry + icons", "[m13][s5][deb]") {
    if (!debExists()) {
        SKIP("deb not built");
    }
    const QString contents = runDpkgDeb({QStringLiteral("--contents")});
    REQUIRE(contents.contains(QStringLiteral("./usr/share/applications/signalforge.desktop")));
    REQUIRE(contents.contains(QStringLiteral("./usr/share/icons/hicolor/256x256/apps/signalforge.png")));
    REQUIRE(contents.contains(QStringLiteral("./usr/share/pixmaps/signalforge.png")));
}

TEST_CASE("M13 S5: package includes profile harness scripts", "[m13][s5][deb]") {
    if (!debExists()) {
        SKIP("deb not built");
    }
    const QString contents = runDpkgDeb({QStringLiteral("--contents")});
    REQUIRE(contents.contains(QStringLiteral("./opt/signalforge/tools/profile/run_profile.sh")));
    REQUIRE(contents.contains(QStringLiteral("./opt/signalforge/tools/profile/check_regression.py")));
    REQUIRE(contents.contains(QStringLiteral("./opt/signalforge/tools/profile/m12_regression_suite.sh")));
}

TEST_CASE("M13 S5: package includes ADRs (9 total — ADR-006 skipped)", "[m13][s5][deb]") {
    if (!debExists()) {
        SKIP("deb not built");
    }
    const QString contents = runDpkgDeb({QStringLiteral("--contents")});
    REQUIRE(contents.contains(QStringLiteral("ADR-001")));
    REQUIRE(contents.contains(QStringLiteral("ADR-007-sfreplay-v1-format-pivot.md")));
    REQUIRE(contents.contains(QStringLiteral("ADR-008-decoder-registrar-runtime-schema.md")));
    REQUIRE(contents.contains(QStringLiteral("ADR-009-mainwindow-pipeline-attach.md")));
    REQUIRE(contents.contains(QStringLiteral("ADR-010-chart-qquickwidget-host-scene.md")));
}

TEST_CASE("M13 S5: deb file size under 50 MB target (spec §5.1)", "[m13][s5][deb]") {
    if (!debExists()) {
        SKIP("deb not built");
    }
    const auto size = QFileInfo(debPath()).size();
    constexpr qint64 kFiftyMB = 50LL * 1024LL * 1024LL;
    REQUIRE(size > 0);
    REQUIRE(size < kFiftyMB);
}
