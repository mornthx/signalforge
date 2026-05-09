// tests/integration/gui/test_release_binary_smoke.cpp
//
// M14 S1 — ctest entry point for the CI release-binary GUI smoke test.
//
// The actual smoke logic lives in the sibling `release_binary_smoke.sh`
// script (per M14-concerns C1: shell + python3 stdlib for portability,
// no PIL/ImageMagick dep). This file is a thin Catch2 wrapper so the
// smoke is discoverable by ctest and runs in the standard test matrix.
//
// Build paths are wired from CMake via `target_compile_definitions`:
//   - SIGNALFORGE_RELEASE_BINARY_PATH  → absolute path to the release
//                                        signalforge executable
//   - SIGNALFORGE_SOURCE_ROOT          → repo root (for examples/schemas/)
//   - SIGNALFORGE_M14_SMOKE_HARNESS    → absolute path to release_binary_smoke.sh
//   - SIGNALFORGE_M14_SMOKE_FIXTURE    → absolute path to m14_smoke.yaml

#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <catch2/catch_test_macros.hpp>

namespace {

QString releaseBinaryPath() {
    return QString::fromUtf8(SIGNALFORGE_RELEASE_BINARY_PATH);
}

QString sourceRoot() {
    return QString::fromUtf8(SIGNALFORGE_SOURCE_ROOT);
}

QString harnessPath() {
    return QString::fromUtf8(SIGNALFORGE_M14_SMOKE_HARNESS);
}

QString fixturePath() {
    return QString::fromUtf8(SIGNALFORGE_M14_SMOKE_FIXTURE);
}

}  // namespace

TEST_CASE("M14 S1: release binary smoke (Tier A + Tier B)", "[m14][s1][gui][smoke]") {
    // Skip when the release binary isn't built — e.g., a Debug-only ctest
    // run. The harness script is per-CI-job sufficient; ctest under
    // `--preset release` still runs it.
    const QString binary = releaseBinaryPath();
    if (binary.isEmpty() || !QFileInfo::exists(binary)) {
        SKIP("release binary not built (path='" + binary.toStdString() + "')");
    }

    // Sanity-check the harness + fixture exist; CMake should have wired
    // these absolute paths at configure time.
    REQUIRE(QFileInfo::exists(harnessPath()));
    REQUIRE(QFileInfo::exists(fixturePath()));
    REQUIRE(QFileInfo::exists(sourceRoot() + "/examples/schemas/temperature_sensor.yaml"));

    QStringList args;
    args << QStringLiteral("--binary") << binary << QStringLiteral("--repo-root") << sourceRoot()
         << QStringLiteral("--fixture") << fixturePath() << QStringLiteral("--timeout") << QStringLiteral("30");

    QProcess proc;
    proc.setProcessChannelMode(QProcess::MergedChannels);
    proc.start(QStringLiteral("bash"), QStringList() << harnessPath() << args);
    REQUIRE(proc.waitForStarted(5000));
    const bool finished = proc.waitForFinished(60000);  // 30 s timeout + slack
    INFO("Harness output (merged stdout+stderr):\n" << proc.readAll().toStdString());
    REQUIRE(finished);
    REQUIRE(proc.exitStatus() == QProcess::NormalExit);
    REQUIRE(proc.exitCode() == 0);
}
