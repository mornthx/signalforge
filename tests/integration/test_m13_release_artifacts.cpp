// tests/integration/test_m13_release_artifacts.cpp
//
// M13 S5 — verifies the **source-tree** files that should ship
// in the V1.0 .deb actually exist before `cmake --build ...
// --target package` is invoked. CI-runnable; catches accidental
// doc deletion / rename without needing the .deb itself.
//
// The complementary `test_m13_deb_package.cpp` checks the
// post-package binary structure.
#include <QFileInfo>
#include <QString>
#include <catch2/catch_test_macros.hpp>

namespace {

constexpr const char* kSourceRoot = SIGNALFORGE_SOURCE_ROOT;

void requireFileExists(const char* relPath) {
    const QString abs = QString::fromUtf8(kSourceRoot) + QStringLiteral("/") + QString::fromUtf8(relPath);
    INFO("expected file: " << abs.toStdString());
    REQUIRE(QFileInfo(abs).exists());
}

}  // namespace

TEST_CASE("M13 S5: V0.1 release docs exist (V1.0 release-notes deferred per V0 charter)", "[m13][s5]") {
    // V0 charter (`docs/V0-series-charter.md`) defers V1.0 indefinitely;
    // `docs/release-notes/v1.0.0.md` was removed at V0.1 close. Replaced
    // here with the V0.1 status summary + V0 charter checks. The
    // historical V1 freeze record (`docs/v1.0-spec-list.md`) is retained
    // (see disclaimer at top of that file).
    requireFileExists("docs/V0-series-charter.md");
    requireFileExists("docs/v0.1-status-summary.md");
    requireFileExists("docs/install.md");
    requireFileExists("docs/v1.0-spec-list.md");
    requireFileExists("docs/m13-hardware-verification.md");
}

TEST_CASE("M13 S5: prior milestone HW verification protocols still exist", "[m13][s5]") {
    requireFileExists("docs/m9-hardware-verification.md");
    requireFileExists("docs/m10-hardware-verification.md");
    requireFileExists("docs/m11-hardware-verification.md");
}

TEST_CASE("M13 S5: V1.0 frozen format spec exists", "[m13][s5]") {
    requireFileExists("docs/format/sfreplay-v1.md");
}

TEST_CASE("M13 S5: ADRs ship with V1.0 (9 ADRs; 006 skipped)", "[m13][s5]") {
    // V1 has 9 ADRs total; numbering jumps 005 → 007 because
    // M7 cycle-detection was documented in done.md not ADR.
    // ADR-008 + ADR-009 added at M13 S7 (V1.0 live-mode plumbing fix).
    // ADR-010 added at M13 S8 (chart QQuickWidget host scene fix).
    // See docs/v1.0-spec-list.md §3 for rationale.
    requireFileExists("docs/architecture/decisions/ADR-001-rendering-approach.md");
    requireFileExists("docs/architecture/decisions/ADR-002-crash-reporting-backend.md");
    requireFileExists("docs/architecture/decisions/ADR-003-metric-name-validation.md");
    requireFileExists("docs/architecture/decisions/ADR-004-signal-buffer-overhead-threshold.md");
    requireFileExists("docs/architecture/decisions/ADR-005-signal-buffer-publish-cadence.md");
    requireFileExists("docs/architecture/decisions/ADR-007-sfreplay-v1-format-pivot.md");
    requireFileExists("docs/architecture/decisions/ADR-008-decoder-registrar-runtime-schema.md");
    requireFileExists("docs/architecture/decisions/ADR-009-mainwindow-pipeline-attach.md");
    requireFileExists("docs/architecture/decisions/ADR-010-chart-qquickwidget-host-scene.md");
}

TEST_CASE("M13 S5: M3-M12 baseline.md files all exist", "[m13][s5]") {
    requireFileExists("tests/benchmark/results/M3-baseline.md");
    requireFileExists("tests/benchmark/results/M4-baseline.md");
    requireFileExists("tests/benchmark/results/M5-baseline.md");
    requireFileExists("tests/benchmark/results/M6-baseline.md");
    requireFileExists("tests/benchmark/results/M7-baseline.md");
    requireFileExists("tests/benchmark/results/M8-baseline.md");
    requireFileExists("tests/benchmark/results/M10-baseline.md");
    requireFileExists("tests/benchmark/results/M11-baseline.md");
    requireFileExists("tests/benchmark/results/M12-baseline.md");
    requireFileExists("tests/benchmark/results/M12-profile-report.md");
    requireFileExists("tests/benchmark/results/M12-regression-suite.md");
}

TEST_CASE("M13 S5: deb scripts + desktop entry + icon exist in source tree", "[m13][s5]") {
    requireFileExists("cmake/install.cmake");
    requireFileExists("cmake/cpack-deb.cmake");
    requireFileExists("cmake/deb-scripts/postinst");
    requireFileExists("cmake/deb-scripts/prerm");
    requireFileExists("installer/signalforge.desktop");
    requireFileExists("installer/signalforge.png");
}

TEST_CASE("M13 S5: top-level LICENSE exists (referenced by CPack)", "[m13][s5]") {
    requireFileExists("LICENSE");
}
