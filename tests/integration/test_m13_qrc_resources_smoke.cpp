// tests/integration/test_m13_qrc_resources_smoke.cpp
//
// M13 S8.2 / ADR-010 — qrc registration smoke test.
//
// Verifies that the ChartHost.qml resource is reachable through Qt's
// resource system after Q_INIT_RESOURCE(qml). Catches the
// static-archive linker-drop regression discovered in S8.2: when
// libsignalforge_app_ui.a contains qrc_qml.cpp.o but the binary
// references no symbol from it, ld silently drops the object and the
// resource never registers — `setSource(qrc:/qml/ChartHost.qml)` then
// fails at runtime with no compile-time signal.
//
// This test is intentionally minimal: it does NOT need a GUI display,
// QApplication, or QQuickWidget. It just calls Q_INIT_RESOURCE and
// verifies QFile can open the embedded path.

#include <QByteArray>
#include <QFile>
#include <QString>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("M13 S8.2 / ADR-010: ChartHost.qml resource registers + opens", "[m13][s8][adr-010][qrc]") {
    // Force registration. In production main.cpp does this at startup;
    // here we do it explicitly so the test is self-contained.
    Q_INIT_RESOURCE(qml);

    QFile chartHost(QStringLiteral(":/qml/ChartHost.qml"));
    INFO("If this REQUIRE fails, qrc_qml.cpp.o was dropped by the linker "
         "(static-archive --gc-sections behavior) and Q_INIT_RESOURCE(qml) "
         "is no longer effective. See ADR-010 §Implementation lesson.");
    REQUIRE(chartHost.exists());
    REQUIRE(chartHost.open(QIODevice::ReadOnly));

    const QByteArray contents = chartHost.readAll();
    REQUIRE(contents.size() > 0);
    REQUIRE(contents.contains("import QtQuick"));
    REQUIRE(contents.contains("anchors.fill: parent"));
}
