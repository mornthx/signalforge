// tests/integration/test_sfreplay_inspect.cpp
//
// M10 S8: invoke the sfreplay_inspect CLI on a fresh writer-output
// fixture and verify the JSON output reflects the file contents.
// Doubles as a third-party reference parser of the format spec
// at docs/format/sfreplay-v1.md (the inspector does its own
// byte-level parse independent of SessionReader).
#include "buffer/signal_buffer_registry.hpp"
#include "decode/decoder_interface.hpp"
#include "session/session_writer.hpp"

#include <QCoreApplication>
#include <QProcess>
#include <QTemporaryDir>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <memory>
#include <nlohmann/json.hpp>

namespace s = signalforge::session;
namespace b = signalforge::buffer;
namespace d = signalforge::decoder;

namespace {

struct InspectFixture {
    InspectFixture() {
        if (!QCoreApplication::instance()) {
            static int argc = 0;
            static char* argv[] = {nullptr};
            app_ = std::make_unique<QCoreApplication>(argc, argv);
        }
    }
    std::unique_ptr<QCoreApplication> app_;
    QTemporaryDir tmp_;
};

d::SignalMetadata makeMeta(const QString& id, d::SignalType type) {
    d::SignalMetadata m;
    m.id = id;
    m.name = id;
    m.unit = QStringLiteral("u");
    m.type = type;
    return m;
}

}  // namespace

TEST_CASE_METHOD(InspectFixture, "S8: sfreplay_inspect reports correct counts on a fresh fixture",
                 "[session][s8][inspect]") {
    const QString recordPath = tmp_.filePath(QStringLiteral("inspect.sfreplay"));

    // Write a small but non-trivial fixture.
    {
        b::SignalBufferRegistry registry;
        s::SessionWriter writer(registry);
        std::vector<d::SignalMetadata> sigs{
            makeMeta(QStringLiteral("voltage"), d::SignalType::Double),
            makeMeta(QStringLiteral("count"), d::SignalType::Int64),
        };
        writer.onSignalsRegistered(QStringLiteral("dev"), sigs);

        REQUIRE(writer.start(recordPath, QStringLiteral("inspect-test"), QStringLiteral("schema-x")));

        const auto t0 = writer.metadata().recordingStart;
        for (int i = 0; i < 5; ++i) {
            writer.onSignal(t0 + std::chrono::microseconds(i * 100), QStringLiteral("voltage"),
                            d::SignalValue{1.0 + i});
        }
        for (int i = 0; i < 3; ++i) {
            writer.onSignal(t0 + std::chrono::microseconds(500 + i * 100), QStringLiteral("count"),
                            d::SignalValue{std::int64_t{i}});
        }
        (void)writer.stop();
    }

    // Locate sfreplay_inspect in the build tree (sibling of this
    // test binary). CMake sets a target-file directory under
    // tools/sfreplay_inspect/.
    const QString cliPath = QStringLiteral(SIGNALFORGE_SFREPLAY_INSPECT_BINARY);
    REQUIRE_FALSE(cliPath.isEmpty());

    // Run with --json
    QProcess proc;
    proc.start(cliPath, {recordPath, QStringLiteral("--json")});
    REQUIRE(proc.waitForFinished(5000));
    REQUIRE(proc.exitCode() == 0);

    const auto stdoutBytes = proc.readAllStandardOutput();
    nlohmann::json j = nlohmann::json::parse(stdoutBytes.toStdString());

    REQUIRE(j["format_version"].get<int>() == 1);
    REQUIRE(j["description"].get<std::string>() == "inspect-test");
    REQUIRE(j["schema_id"].get<std::string>() == "schema-x");
    REQUIRE(j["initial_signal_count"].get<int>() == 2);
    REQUIRE(j["records_parsed"].get<int>() == 8);  // 5 + 3 signal-value records
    REQUIRE(j["record_histogram"]["1_signal_value"].get<int>() == 8);
    REQUIRE(j["record_histogram"]["2_catalog_extension"].get<int>() == 0);
    REQUIRE(j["footer_present"].get<bool>() == true);
    REQUIRE(j["footer_total_records"].get<int>() == 8);
    REQUIRE(j["file_complete"].get<bool>() == true);

    // Signal catalog content
    const auto& sigs = j["signals"];
    REQUIRE(sigs.size() == 2);
    REQUIRE(sigs[0]["id"].get<std::string>() == "voltage");
    REQUIRE(sigs[0]["type"].get<std::string>() == "Double");
    REQUIRE(sigs[1]["id"].get<std::string>() == "count");
    REQUIRE(sigs[1]["type"].get<std::string>() == "Int64");
}

TEST_CASE_METHOD(InspectFixture, "S8: sfreplay_inspect reports incomplete on truncated fixture",
                 "[session][s8][inspect]") {
    const QString recordPath = tmp_.filePath(QStringLiteral("trunc-inspect.sfreplay"));

    // Write a small file then chop the footer off.
    {
        b::SignalBufferRegistry registry;
        s::SessionWriter writer(registry);
        std::vector<d::SignalMetadata> sigs{makeMeta(QStringLiteral("x"), d::SignalType::Double)};
        writer.onSignalsRegistered(QStringLiteral("d"), sigs);
        REQUIRE(writer.start(recordPath));
        writer.onSignal(writer.metadata().recordingStart, QStringLiteral("x"), d::SignalValue{1.0});
        (void)writer.stop();
    }
    QFile f(recordPath);
    REQUIRE(f.open(QIODevice::ReadWrite));
    REQUIRE(f.resize(f.size() - 16));  // drop the footer
    f.close();

    const QString cliPath = QStringLiteral(SIGNALFORGE_SFREPLAY_INSPECT_BINARY);
    QProcess proc;
    proc.start(cliPath, {recordPath, QStringLiteral("--json")});
    REQUIRE(proc.waitForFinished(5000));
    REQUIRE(proc.exitCode() == 0);  // missing footer is non-fatal

    const auto stdoutBytes = proc.readAllStandardOutput();
    nlohmann::json j = nlohmann::json::parse(stdoutBytes.toStdString());
    REQUIRE(j["footer_present"].get<bool>() == false);
    REQUIRE(j["file_complete"].get<bool>() == false);
}
