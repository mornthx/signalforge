// tests/unit/inspect/raw_packet_view_test.cpp
//
// M31 S2 — RawPacketView: packet-list population from a tap, display-filter
// narrowing, and hex-pane update on row selection.

#include "frame/raw_frame.hpp"
#include "inspect/raw_frame_tap.hpp"
#include "inspect/raw_packet_view.hpp"

#include <QApplication>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <catch2/catch_test_macros.hpp>

namespace insp = signalforge::inspect;
namespace frame = signalforge::frame;

namespace {

QApplication& app() {
    static int argc = 1;
    static char arg0[] = "test";
    static char* argv[] = {arg0, nullptr};
    static QApplication instance(argc, argv);
    return instance;
}

frame::RawFrame makeFrame(const QString& source, QByteArray payload, std::uint64_t seq) {
    frame::RawFrame f;
    f.sourceId = source;
    f.payload = std::move(payload);
    f.protocolHint = QStringLiteral("udp");
    f.recvAt = std::chrono::steady_clock::now();
    f.sequenceNumber = seq;
    return f;
}

}  // namespace

TEST_CASE("M31 S2: packet view lists captured frames", "[inspect][m31][rawview]") {
    app();
    insp::RawFrameTap tap;
    tap.onFrame(makeFrame(QStringLiteral("udp:rig"), QByteArray("\x01\x02\x03", 3), 1));
    tap.onFrame(makeFrame(QStringLiteral("tcp:dev"), QByteArray("\xff\xee", 2), 2));

    insp::RawPacketView view(tap);
    view.refresh();
    CHECK(view.totalRowCount() == 2);
    CHECK(view.visibleRowCount() == 2);
    CHECK(view.filterValid());

    // New frames are picked up incrementally.
    tap.onFrame(makeFrame(QStringLiteral("udp:rig"), QByteArray("\x04", 1), 3));
    view.refresh();
    CHECK(view.totalRowCount() == 3);
}

TEST_CASE("M31 S2: typing a filter narrows packets", "[inspect][m31][rawview][interaction]") {
    app();
    insp::RawFrameTap tap;
    tap.onFrame(makeFrame(QStringLiteral("udp:rig"), QByteArray("\x01\x02\x03\x04\x05", 5), 1));
    tap.onFrame(makeFrame(QStringLiteral("tcp:dev"), QByteArray("\xff", 1), 2));
    tap.onFrame(makeFrame(QStringLiteral("udp:rig"), QByteArray("\xaa\xbb", 2), 3));

    insp::RawPacketView view(tap);
    view.refresh();
    REQUIRE(view.totalRowCount() == 3);

    view.filterEdit()->setText(QStringLiteral("source == udp:rig"));
    CHECK(view.filterValid());
    CHECK(view.visibleRowCount() == 2);

    view.filterEdit()->setText(QStringLiteral("len > 2"));
    CHECK(view.visibleRowCount() == 1);  // only the 5-byte frame

    view.filterEdit()->setText(QStringLiteral("hex contains ff"));
    CHECK(view.visibleRowCount() == 1);  // only the 0xff frame

    view.filterEdit()->setText(QStringLiteral("len >"));  // parse error
    CHECK_FALSE(view.filterValid());
    CHECK(view.visibleRowCount() == 3);  // invalid filter shows all

    view.filterEdit()->setText(QString());
    CHECK(view.visibleRowCount() == 3);
}

TEST_CASE("M31 S2: selecting a packet shows its hex dump", "[inspect][m31][rawview][interaction]") {
    app();
    insp::RawFrameTap tap;
    tap.onFrame(makeFrame(QStringLiteral("udp:rig"), QByteArray("\xde\xad\xbe\xef", 4), 1));
    insp::RawPacketView view(tap);
    view.refresh();
    REQUIRE(view.totalRowCount() == 1);

    CHECK(view.hexView()->toPlainText().isEmpty());
    view.selectRow(0);
    const QString dump = view.hexView()->toPlainText();
    CHECK(dump.contains(QStringLiteral("de ad be ef")));
}
