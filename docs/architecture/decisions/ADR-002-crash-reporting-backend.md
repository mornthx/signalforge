# ADR-002 — Crash Reporting Backend

**Status**: Accepted
**Date**: 2026-04-23
**Context**: M2 S2 HALT — Crashpad has no upstream CMake support

## Decision

Use sentry-native as V1's crash reporting backend. Previous specification (architecture v0.4 through v0.6) named Crashpad; that choice is superseded.

## Supporting Evidence

M2 Phase 5 execution HALTed at subtask S2 (commit 8891b20 on milestone/M2) when FetchContent against `chromium.googlesource.com/crashpad/crashpad` produced no CMake targets. Upstream Crashpad is GN-only (Google's internal build system), requiring either a community CMake fork of uncertain maintenance status or manual vendoring of sources with hand-authored CMakeLists.

sentry-native provides:

- Native CMake support (CMakeLists.txt in the project root, standard target exports)
- MIT license (architecture compatibility unchanged from Crashpad's Apache 2.0)
- Local minidump generation without requiring a backend service (matches architecture §14.3 "no upload backend")
- Active upstream maintenance by Sentry.io
- Comparable feature set for the V1 use case: minidumps on crash, handler process isolation

At the architecture level, the two libraries are interchangeable for V1's requirements. The decision is driven by integration cost, not by capability.

## Consequences

1. Architecture §4.1, §13.2, §14.3 updated to reference sentry-native (see v0.7 Change Log entry).
2. M2 spec §4.5.3 `crash_reporting.hpp` interface remains semantically identical (init / shutdown / active). The `CrashReporterConfig.handlerExecutable` field becomes implementation-specific and may be repurposed or dropped by the M2 implementer.
3. `cmake/dependencies.cmake` adds a FetchContent declaration for sentry-native at a pinned tag (M2 S2 implementer selects the tag and records in the done report).
4. V2 gains the option to enable Sentry.io upload with zero library change — only configuration.

## Considered Alternatives

- **Community Crashpad CMake fork** (e.g., TheAssassin/libcrashpad-cmake, getsentry/crashpad): fork maintenance and upstream divergence risk. Rejected.
- **Vendor Crashpad sources + hand-author CMake**: very high initial cost and ongoing maintenance burden as Crashpad evolves. Rejected.
- **Defer crash reporting to M2.5 or M10**: narrows M2 scope, removes a foundation-layer capability that integration tests and early hardware bring-up may benefit from. Rejected.

## Open Items

- M2 S2 implementer pins a specific sentry-native release tag in `cmake/dependencies.cmake`; tag is recorded in `.claude/M2-done.md`.
- Crash-trigger tool (`tools/crash_test/`) uses sentry-native API equivalents for the crash paths.

## Revisit Trigger

If V2 introduces a server-side crash aggregation requirement and sentry-native's Sentry.io integration becomes a policy issue (e.g., data residency), re-evaluate. No near-term trigger expected.
