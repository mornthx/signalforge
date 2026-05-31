// tests/unit/workbench/signal_identity_test.cpp
//
// M33 S2 — SignalIdentity: stable palette indices + quality derivation.

#include "workbench/signal_identity.hpp"

#include <QColor>
#include <catch2/catch_test_macros.hpp>

namespace wb = signalforge::workbench;
using namespace std::chrono_literals;

TEST_CASE("M33 S2: palette index is stable per signal and assigned in order", "[workbench][m33]") {
    wb::SignalIdentity ident;
    CHECK_FALSE(ident.isKnown(QStringLiteral("a")));

    const int a = ident.colorIndex(QStringLiteral("a"));
    const int b = ident.colorIndex(QStringLiteral("b"));
    CHECK(a == 0);
    CHECK(b == 1);
    CHECK(ident.colorIndex(QStringLiteral("a")) == a);  // stable
    CHECK(ident.isKnown(QStringLiteral("a")));
    CHECK(ident.count() == 2);
}

TEST_CASE("M33 S2: indices wrap at the palette size", "[workbench][m33]") {
    wb::SignalIdentity ident;
    for (int i = 0; i < wb::SignalIdentity::kPaletteSize; ++i) {
        CHECK(ident.colorIndex(QStringLiteral("s%1").arg(i)) == i);
    }
    // The 9th distinct signal wraps to index 0.
    CHECK(ident.colorIndex(QStringLiteral("wrap")) == 0);
    CHECK(ident.count() == wb::SignalIdentity::kPaletteSize + 1);
}

TEST_CASE("M34 P5: signals of one driver share a colour index", "[workbench][m34]") {
    wb::SignalIdentity ident;
    CHECK(wb::SignalIdentity::driverKeyOf(QStringLiteral("udp:devA/temperature")) == QStringLiteral("udp:devA"));
    CHECK(wb::SignalIdentity::driverKeyOf(QStringLiteral("plainid")) == QStringLiteral("plainid"));

    const int t = ident.colorIndex(QStringLiteral("udp:devA/temperature"));
    const int p = ident.colorIndex(QStringLiteral("udp:devA/pressure"));
    CHECK(t == p);              // same driver → same colour
    CHECK(ident.count() == 1);  // one driver assigned, not two signals

    const int other = ident.colorIndex(QStringLiteral("udp:devB/rpm"));
    CHECK(other != t);  // a different driver gets the next index
    CHECK(ident.count() == 2);
    CHECK(ident.isKnown(QStringLiteral("udp:devA/anything")));  // keyed by driver
}

TEST_CASE("M34 P5: a per-signal override beats the driver default", "[workbench][m34]") {
    wb::SignalIdentity ident;
    const QString id = QStringLiteral("udp:devA/temperature");
    CHECK_FALSE(ident.hasOverride(id));
    CHECK_FALSE(ident.overrideColor(id).has_value());

    ident.setOverrideColor(id, QColor(0xAB, 0xCA, 0xBC));
    CHECK(ident.hasOverride(id));
    REQUIRE(ident.overrideColor(id).has_value());
    CHECK(ident.overrideColor(id)->name() == QColor(0xAB, 0xCA, 0xBC).name());
    // The sibling signal (same driver) is unaffected by the override.
    CHECK_FALSE(ident.hasOverride(QStringLiteral("udp:devA/pressure")));

    ident.clearOverrideColor(id);
    CHECK_FALSE(ident.hasOverride(id));
}

TEST_CASE("M33 S2: quality derives from age and decode error", "[workbench][m33]") {
    const auto stale = 1s;
    const auto bad = 5s;
    CHECK(wb::qualityFromAge(100ms, stale, bad) == wb::Quality::Good);
    CHECK(wb::qualityFromAge(2s, stale, bad) == wb::Quality::Stale);
    CHECK(wb::qualityFromAge(10s, stale, bad) == wb::Quality::Bad);
    CHECK(wb::qualityFromAge(100ms, stale, bad, /*decodeError=*/true) == wb::Quality::Bad);

    CHECK(wb::qualityName(wb::Quality::Good) == QStringLiteral("good"));
    CHECK(wb::qualityName(wb::Quality::Uncertain) == QStringLiteral("uncertain"));
}
