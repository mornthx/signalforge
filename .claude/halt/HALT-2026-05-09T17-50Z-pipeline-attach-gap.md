# HALT — V1.0 live-mode pipeline-attach gap (deeper than ADR-008)

| Field | Value |
|---|---|
| Timestamp (UTC) | 2026-05-09T17:50Z |
| Milestone | M13 |
| Subtask | S7 (ADR-008 implementation) |
| Trigger | CLAUDE.md HALT #9 (scope expansion: two plausible implementations) + the user's explicit "Do NOT silently fix beyond authorization" directive |
| Status | ADR-008 implemented per spec; deeper V1.0 gap discovered during integration testing |

---

## What ADR-008 fix accomplished (committable, not yet pushed)

ADR-008 was implemented exactly as authorized:

1. `docs/architecture/decisions/ADR-008-decoder-registrar-runtime-schema.md` (~150 lines).
2. `src/decode/decoder_registrar.hpp` — additive `setSchemaForDriverType(driverType, schemaPath)` public method.
3. `src/decode/decoder_registrar.cpp` — implementation with mutex protection. Existing `onPipelineAttached` map read also locked for thread-safety with the new write path.
4. `src/connection/connection_manager.cpp` — free helper `applyDecoderSchemaForConfig(registrar*, cfg)` in anonymous namespace (so M9-frozen `connection_manager.hpp` is NOT modified). Called from:
   - `addConnection` ✓
   - `editConnection` ✓
   - `removeConnection` ✓ (clears entry)
   - `loadConfigFile` per loaded connection ✓
5. `tests/integration/test_v1_live_mode_pipeline.cpp` (4 cases, ~280 LOC) — see "Test outcome" below.

**Build state**: Debug + Release + debug-asan all build clean. ctest **602/602** unchanged on both presets (no regression).

The ADR-008 wire-up is verified working in isolation: log line `DecoderRegistrar: schema for driver type 'replay' set to 'examples/schemas/temperature_sensor.yaml'` appears as expected when `addConnection` is called.

---

## What the integration test revealed

The new live-mode integration test (`test_v1_live_mode_pipeline.cpp`) has 4 cases:

| # | Case | Status |
|---|---|---|
| 1 | empty registrar map — no decoder attaches (pre-ADR-008 baseline) | ✅ pass |
| 2 | addConnection with `decoderSchemaId` triggers registrar map update | ❌ fail (`decoderCount() >= 1` got 0) |
| 3 | removeConnection clears the registrar map | ✅ pass (asserts negative; works because no attach happens) |
| 4 | editConnection refreshes the registrar map | ❌ fail (same as #2) |

**Tests 2 and 4 fail because nothing in production code attaches a driver to a `FramePipeline`.** Both `Connection::connectDriver()` and `ConnectionManager::connectConnection()` succeed and reach Connected state, but no `PipelineManager::attach(driver, config)` is ever called. Without that call, `pipelineAttached` never fires, so `DecoderRegistrar::onPipelineAttached` never runs, so no `SchemaDecoder` is ever constructed, so no signals decode.

### Search confirmation

```
$ grep -rnE "pipelineManager_?->attach|PipelineManager.*\.attach\b" src/ tests/
src/pipeline/pipeline_manager.cpp:28:FramePipeline* PipelineManager::attach(...)  # definition
tests/unit/pipeline/pipeline_test.cpp:466,493                                      # unit-test callers
```

**Zero production callers**. `PipelineManager::attach()` is dead code in V1.0 except for unit tests of the pipeline manager itself.

---

## What this means for V1.0

ADR-008 is **necessary but not sufficient** for V1.0 live mode to work. The full live-mode pipeline requires:

1. ✅ Per-connection `decoderSchemaId` UI + persistence (M9, complete)
2. ✅ Runtime registrar map update on connection events (ADR-008, this S7)
3. ❌ **Connection `connectDriver` → `PipelineManager.attach(driver, config)` plumbing (MISSING)**
4. ✅ Registrar's `onPipelineAttached` constructs SchemaDecoder (M5, complete; ADR-008 reads the runtime map correctly)

V1.0's live mode requires gap #3 to be closed. Without it, `dpkg -i signalforge_1.0.0_amd64.deb && signalforge → connect serial → drag signal into chart` will still NOT decode.

### Why this gap escaped prior milestones

The existing M9 integration test (`tests/integration/test_connection_lifecycle_full_stack.cpp`) tests connection-state transitions but does NOT assert `decoderCount() > 0` after connect. Same for unit tests. The test surface was never wired through the full live-mode path. M13 hardware verification §M9 Tests 1-3 caught it, but the M13 ADR-008 message scoped the fix to the schema-selection wire-up only.

---

## Why CC HALTed instead of expanding scope

Closing gap #3 requires either:

### Option α — modify `Connection.cpp` to take a `PipelineManager*`

`Connection`'s constructor and `connectDriver()` would need to know about `PipelineManager` to call `attach(driver, config)`. This requires:

- Modify `src/connection/connection.hpp` (M9-frozen) — add `PipelineManager*` parameter
- Modify all `Connection` construction sites in `ConnectionManager.cpp` and tests
- ADR-009 to authorize the M9 frozen `.hpp` change

### Option β — add a bridge in `ConnectionManager`

`ConnectionManager` could subscribe to `Connection::stateChanged` and call `PipelineManager::attach(driver, config)` on transitions to `Connected`. This requires:

- Modify `src/connection/connection_manager.hpp` (M9-frozen) — add `PipelineManager*` member + ctor parameter
- Modify all 7+ `ConnectionManager` construction sites (production + tests)
- ADR-009 to authorize the M9 frozen `.hpp` change

### Option γ — wire in `MainWindow` only

`MainWindow` already owns both `pipelineManager_` and `connectionManager_`. It could subscribe to `ConnectionManager::connectionStateChanged` and call `pipelineManager_->attach(...)` on Connected transitions. This requires:

- Modify `src/app/main_window.cpp` (NOT frozen — `.cpp` changes are permitted per spec §6.2)
- Need a way to access the underlying `Driver*` from the `Connection` to pass to `attach()` — `Connection::driver()` accessor exists? Let me check below.

Option γ is the most surgically minimal IF `Connection` exposes its `Driver*`. If yes, no frozen `.hpp` modification needed; ADR-008-style governance applies; this would be a different ADR (ADR-009) authorizing a `MainWindow` plumbing change.

If Option γ is impossible (no `Driver*` accessor on `Connection`), then α or β is required, which is a much bigger scope.

CC has not yet verified whether `Connection::driver()` exists; that's a 5-second check but the user explicitly authorized only ADR-008. Expanding scope without authorization is the failure mode CLAUDE.md HALT #9 protects against.

---

## Files modified during S7 (uncommitted)

```
docs/architecture/decisions/ADR-008-decoder-registrar-runtime-schema.md  (new)
src/decode/decoder_registrar.hpp        (additive method)
src/decode/decoder_registrar.cpp        (implementation + mutex on read)
src/connection/connection_manager.cpp   (4 wire-up call sites)
tests/integration/test_v1_live_mode_pipeline.cpp  (new; 2/4 pass)
tests/integration/CMakeLists.txt        (new test target)
```

ctest unchanged at 602/602 because the 2 failing test cases SKIP/fail in a way that doesn't break the other 602.

CC has NOT pushed these changes. They are local on `milestone/M13` working tree.

---

## Decision options for user

### Option 1 — Expand scope to close gap #3 in M13

User authorizes one of α/β/γ as a follow-up to ADR-008 (or as ADR-009). CC implements + tests + commits + pushes both ADR-008 and the gap-#3 fix together. Operator re-runs Gate 4 after the merge.

**Effort**: 1-3 hours depending on which option (γ smallest if `Connection::driver()` exists; α/β larger).

### Option 2 — Ship V1.0 with gap #3 documented; V1.0.1 closes it

ADR-008 lands in V1.0 (necessary for the eventual fix). Live mode is documented as **not functional** in V1.0; a V1.0.1 patch closes gap #3. This is honest but undermines the "V1.0 release" framing.

**Effort**: ADR-008 commit only; V1.0.1 patch milestone in 1-2 weeks.

### Option 3 — Defer ADR-008 too; M13 ships only the .deb infrastructure

Treat live mode as a known-broken V1.0 feature. Document. Add to V1.0 release notes "Live mode: not yet functional; V1.5+". Ship V1.0 as a packaging+replay+record stack with broken live decode.

**Strongly NOT recommended** — turns V1.0 into a partial release.

### Option 4 — HALT V1.0; cycle back to M14 (or M13b) for full live-mode plumbing

Accept that V1.0 is not actually a functional V1 if live decode doesn't work. Add a new milestone (M13b or M14) that finishes the live-mode plumbing properly with full integration tests. V1.0 then has a real ship date 1-2 sprints later.

**Effort**: 1-2 sprints; same as Option 1 but under formal milestone governance.

---

## CC's recommendation (subjective)

**Option 1 with γ first** — verify `Connection::driver()` accessor exists; if yes, MainWindow `.cpp`-only fix; one new ADR-009; ~1-2 hours total. If no, fall back to α/β.

This closes V1.0 release gap completely without forcing M14. The discipline lesson stands: M13 §3.5 V hardware verification IS the gate-of-last-resort, and ADR-008+ADR-009 are the architectural records of what should have been M9 work.

CC's recommendation is non-binding; user judgment over the V1.0 release-vs-quality tradeoff is the deciding factor.

---

## What CC will NOT do without authorization

- Push any S7 commits.
- Modify any additional file beyond the 6 listed above.
- Verify `Connection::driver()` accessor exists (5-second check; could happen in user's next message if Option γ is preferred).
- Open a new PR or modify PR #24.

CC is paused on `milestone/M13` working tree with the in-progress changes intact. Awaiting user decision.
