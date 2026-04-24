// tests/integration/test_app_smoke.cpp
#include <QByteArray>
#include <QProcess>
#include <QProcessEnvironment>
#include <QString>
#include <QStringList>
#include <catch2/catch_test_macros.hpp>
#include <cstdlib>
#include <filesystem>
#include <string>

namespace {

std::filesystem::path uniqueXdgRoot() {
    return std::filesystem::temp_directory_path() / ("signalforge_app_smoke_" + std::to_string(::getpid()));
}

QString resolveAppBinary() {
    // The signalforge executable is built into build/<preset>/src/app/signalforge.
    // The Catch2 test runner has its cwd set to the test binary's dir by
    // catch_discover_tests; walk up to locate the app binary.
    //
    // Expected layout:
    //   build/<preset>/
    //     src/app/signalforge
    //     tests/integration/test_app_smoke            <-- $PWD for discovery
    //
    // The smoke-test binary is `tests/integration/test_app_smoke`; the app
    // binary is three directories up at `src/app/signalforge`.

    // Preferred: use a CMake-provided compile definition if available.
#ifdef SIGNALFORGE_APP_BINARY
    return QString::fromUtf8(SIGNALFORGE_APP_BINARY);
#else
    return QStringLiteral("../../src/app/signalforge");
#endif
}

}  // namespace

TEST_CASE("app smoke: launches, initializes, and exits cleanly", "[integration][app_smoke]") {
    const auto xdgRoot = uniqueXdgRoot();
    std::filesystem::remove_all(xdgRoot);

    QProcess proc;
    auto env = QProcessEnvironment::systemEnvironment();
    env.insert("QT_QPA_PLATFORM", "offscreen");
    env.insert("XDG_STATE_HOME", QString::fromStdString(xdgRoot.string()));
    proc.setProcessEnvironment(env);
    proc.setProgram(resolveAppBinary());
    proc.setArguments({QStringLiteral("--headless-smoke-test")});

    proc.start();
    REQUIRE(proc.waitForStarted(5000));
    REQUIRE(proc.waitForFinished(5000));

    const int exitCode = proc.exitCode();
    const auto exitStatus = proc.exitStatus();
    const QByteArray stderrOut = proc.readAllStandardError();

    INFO("stderr output was: " << stderrOut.toStdString());

    REQUIRE(exitStatus == QProcess::NormalExit);
    REQUIRE(exitCode == 0);
    // spdlog's default ERROR token in uppercase (if stderr sink were enabled)
    // must not appear; our setup only writes to the rotating file sink, so
    // stderr is expected to be empty.
    REQUIRE(!stderrOut.contains("ERROR"));

    // Log file must have been created under XDG_STATE_HOME/signalforge/logs/.
    const auto logFile = xdgRoot / "signalforge" / "logs" / "signalforge.log";
    REQUIRE(std::filesystem::exists(logFile));
    REQUIRE(std::filesystem::file_size(logFile) > 0);

    std::filesystem::remove_all(xdgRoot);
}
