// tests/unit/session/session_smoke_test.cpp
//
// S1 smoke test: verifies the M10 freeze-surface headers compile,
// the static lib links, and basic value types are constructable.
// Full lifecycle / encoder / queue / round-trip tests land in
// S3 / S4 / S5 / S7.
#include "session/session_metadata.hpp"
#include "session/session_writer.hpp"

#include <catch2/catch_test_macros.hpp>

namespace s = signalforge::session;

TEST_CASE("S1: RecordingState enum values are distinct", "[session][s1][smoke]") {
    REQUIRE(static_cast<int>(s::RecordingState::Idle) != static_cast<int>(s::RecordingState::Recording));
    REQUIRE(static_cast<int>(s::RecordingState::Recording) != static_cast<int>(s::RecordingState::Error));
    REQUIRE(static_cast<int>(s::RecordingState::Idle) != static_cast<int>(s::RecordingState::Error));
}

TEST_CASE("S1: SessionMetadata defaults are sane", "[session][s1][smoke]") {
    s::SessionMetadata m;
    REQUIRE(m.description.isEmpty());
    REQUIRE(m.decoderSchemaId.isEmpty());
    REQUIRE_FALSE(m.recordingEnd.has_value());
    REQUIRE(m.signalCatalog.empty());
}
