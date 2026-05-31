// tests/unit/inspect/raw_packet_view_test.cpp
//
// M31 S2 — RawPacketView: packet-list population from a tap, display-filter
// narrowing, and hex-pane update on row selection.

#include "decode/frame_dissector.hpp"
#include "decode/schema.hpp"
#include "frame/raw_frame.hpp"
#include "inspect/raw_frame_tap.hpp"
#include "inspect/raw_packet_view.hpp"

#include <QApplication>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QTreeWidget>
#include <QTreeWidgetItem>
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

namespace {

// A tiny 4-byte schema: magic 0xAA, uint8 'a' at 1, int16 'b' at 2.
signalforge::decoder::Schema miniSchema() {
    namespace dec = signalforge::decoder;
    dec::Layout layout;
    layout.name = QStringLiteral("mini");
    layout.endianness = dec::Endianness::Little;
    layout.match.bytes = {0xAA};
    layout.minPayloadBytes = 4;
    dec::FieldDef a;
    a.name = QStringLiteral("a");
    a.offset = 1;
    a.encoding = dec::FieldEncoding::Uint8;
    a.sizeBytes = 1;
    dec::FieldDef b;
    b.name = QStringLiteral("b");
    b.offset = 2;
    b.encoding = dec::FieldEncoding::Int16;
    b.sizeBytes = 2;
    layout.fields = {a, b};
    dec::Schema schema;
    schema.layouts = {layout};
    return schema;
}

}  // namespace

TEST_CASE("M34 P3: selecting a packet builds the dissection tree and highlights bytes",
          "[inspect][m34][rawview][interaction]") {
    app();
    insp::RawFrameTap tap;
    // magic AA, a=7, b=0x0102=258 (LE 02 01).
    tap.onFrame(makeFrame(QStringLiteral("udp:rig"), QByteArray::fromHex("AA070201"), 1));
    insp::RawPacketView view(tap);

    const signalforge::decoder::FrameDissector dissector(miniSchema());
    view.setDissectorProvider([&dissector](const QString& source) -> const signalforge::decoder::FrameDissector* {
        return source.startsWith(QStringLiteral("udp")) ? &dissector : nullptr;
    });
    view.refresh();
    REQUIRE(view.totalRowCount() == 1);

    view.selectRow(0);

    // Tree is populated: magic + the two fields.
    auto* tree = view.dissectionTree();
    REQUIRE(tree->topLevelItemCount() == 3);
    bool sawA = false;
    bool sawB = false;
    for (int i = 0; i < tree->topLevelItemCount(); ++i) {
        const QString name = tree->topLevelItem(i)->text(0);
        if (name == QStringLiteral("a")) {
            sawA = true;
            CHECK(tree->topLevelItem(i)->text(1) == QStringLiteral("7"));
        }
        if (name == QStringLiteral("b")) {
            sawB = true;
            CHECK(tree->topLevelItem(i)->text(1) == QStringLiteral("258"));
        }
    }
    CHECK(sawA);
    CHECK(sawB);

    // Selecting the "b" field (bytes 2–3) highlights bytes in the hex pane.
    CHECK(view.hexView()->extraSelections().isEmpty());
    for (int i = 0; i < tree->topLevelItemCount(); ++i) {
        if (tree->topLevelItem(i)->text(0) == QStringLiteral("b")) {
            tree->setCurrentItem(tree->topLevelItem(i));
            break;
        }
    }
    CHECK_FALSE(view.hexView()->extraSelections().isEmpty());  // hex + ascii spans
}

TEST_CASE("M34 P3: display filter narrows on dissected field values", "[inspect][m34][rawview][interaction]") {
    app();
    insp::RawFrameTap tap;
    // Three frames: b = 258, 5, 258. a = 7, 9, 7.
    tap.onFrame(makeFrame(QStringLiteral("udp:rig"), QByteArray::fromHex("AA070201"), 1));  // b=258
    tap.onFrame(makeFrame(QStringLiteral("udp:rig"), QByteArray::fromHex("AA090500"), 2));  // b=5
    tap.onFrame(makeFrame(QStringLiteral("udp:rig"), QByteArray::fromHex("AA070201"), 3));  // b=258
    insp::RawPacketView view(tap);

    const signalforge::decoder::FrameDissector dissector(miniSchema());
    view.setDissectorProvider([&dissector](const QString&) { return &dissector; });
    view.refresh();
    REQUIRE(view.totalRowCount() == 3);

    // Filter on a dissected numeric field.
    view.filterEdit()->setText(QStringLiteral("b > 100"));
    CHECK(view.filterValid());
    CHECK(view.visibleRowCount() == 2);  // the two b=258 frames

    view.filterEdit()->setText(QStringLiteral("a == 9"));
    CHECK(view.visibleRowCount() == 1);

    // Builtin and dissected field combined.
    view.filterEdit()->setText(QStringLiteral("len == 4 && b == 5"));
    CHECK(view.visibleRowCount() == 1);

    // Clearing restores all rows.
    view.filterEdit()->setText(QString());
    CHECK(view.visibleRowCount() == 3);
}

TEST_CASE("M34 P3: without a dissector, the tree shows a raw-bytes placeholder",
          "[inspect][m34][rawview][interaction]") {
    app();
    insp::RawFrameTap tap;
    tap.onFrame(makeFrame(QStringLiteral("serial:dev"), QByteArray::fromHex("AA070201"), 1));
    insp::RawPacketView view(tap);
    view.refresh();
    view.selectRow(0);

    auto* tree = view.dissectionTree();
    REQUIRE(tree->topLevelItemCount() == 1);  // single placeholder row
    CHECK(tree->topLevelItem(0)->text(0).contains(QStringLiteral("No decoder schema")));
}

TEST_CASE("M34 P5: setFilterText populates the visible bar and applies the filter",
          "[inspect][m34][rawview][interaction]") {
    app();
    insp::RawFrameTap tap;
    tap.onFrame(makeFrame(QStringLiteral("udp:127.0.0.1:9000"), QByteArray("\x01\x02", 2), 1));
    tap.onFrame(makeFrame(QStringLiteral("udp:127.0.0.1:9001"), QByteArray("\x03", 1), 2));
    insp::RawPacketView view(tap);
    view.refresh();
    REQUIRE(view.totalRowCount() == 2);

    // A cross-tier drill-through sets the bar text (unlike setFilter); the user
    // sees and can edit the filter that narrowed the list.
    view.setFilterText(QStringLiteral("source == \"udp:127.0.0.1:9000\""));
    CHECK(view.filterEdit()->text() == QStringLiteral("source == \"udp:127.0.0.1:9000\""));
    CHECK(view.filterValid());
    CHECK(view.visibleRowCount() == 1);

    // Re-applying the same text is a no-op-safe path (guarded re-entry).
    view.setFilterText(QStringLiteral("source == \"udp:127.0.0.1:9000\""));
    CHECK(view.visibleRowCount() == 1);
}
