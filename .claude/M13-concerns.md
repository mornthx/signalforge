# M13 — Concerns

Six concerns surfaced during Phase 4 understanding. Each carries
an implementation-level resolution; **none currently requires
ADR-008**. M13 is V1's final milestone (packaging-only); the
default expectation is **0 ADR-008**, with the C5 "v1.0.0 tag
finality" caveat that any post-tag finding becomes V1.0.1
patch territory under a new ADR.

---

## C1 — Two-class deliverable structure (CC vs operator-blocking)

### Statement

M13's spec §2.1 mixes deliverables that **CC can author /
build / test** with deliverables that **only an operator can
execute / verify**:

CC-blocking (this milestone's commits):
- CMake CPack configuration + `install()` rules
- Post-install / pre-removal scripts
- Documentation suite (release notes, install, spec list,
  combined HW protocol)
- 30-min memory soaks (operator-runnable too, but CC can
  start them; ~30 min wall-clock each)
- M13 integration tests for `.deb` structural validation
- M13-done.md + V1.0 freeze sha256 collection

Operator-blocking (cannot be CC-completed):
- 18-test HW verification with **real** Serial / TCP / UDP
  / Replay connections + GUI session
- DEB install on a **clean** Ubuntu 24.04 VM (CC's dev host
  is "dirty" — has all build deps, prior build artefacts)
- GitHub Release publish via `gh release create` under the
  operator's GitHub identity (Phase 3, next session)
- `v1.0.0` tag (Phase 3, next session)

### Resolution

Plan §1 subtasks S0-S6 are **CC-blocking**; Phase 3 S7 (next
session) covers the remaining operator-blocking work after
Phase 2 approval.

CC-doable parts of operator-blocking deliverables (the `.deb`
structural validation in S5, the soak runs in S4) ship in
M13's commits; the operator-only parts (clean-VM install, real
hardware connections) are surfaced in M13-done.md's "Release
prerequisite status" table with explicit `operator-pending`
markers.

This split prevents CC from claiming verification of work it
cannot actually verify. M13-done.md tells the reviewer
honestly: "CC has shipped the infrastructure; operator must
run X / Y / Z before V1.0 ship."

---

## C2 — Soak runs in M13 timeline

### Statement

Spec §3.5 V mandates two 30-min memory soaks
(`bench_session_writer --soak 1800` for M10 and
`bench_replay --memory-soak 1800` for M11). Total 60 min if
sequential; 30 min if parallel — but parallel runs would share
CPU / memory bandwidth, potentially skewing leak-detection
data.

### Resolution

**Sequential** runs to keep data clean:
1. M10 soak first (30 min). Run with `run_in_background=true`
   so CC can author S2 / S3 documentation while the soak
   completes.
2. M11 soak second (30 min). Same pattern; CC continues
   documentation work in parallel.

If either soak fails (> 10 % VmRSS growth per spec §5.3),
**HALT trigger H5** fires per plan §3:
- Report: which soak failed (M10 vs M11), final growth %,
  approximate time-to-leak signature
- Wait for human Phase 4-style input (per the user's S4
  failure-handling guidance):
  a. ADR-008 + `.cpp`-only hotfix in M13 (exception path)
  b. Defer to V1.0.1 patch milestone post-tag
  c. Re-run with different parameters to confirm
- Do NOT silently fix; do NOT proceed to S5 until decision

Most likely failure mode if it occurs (operator's prior
guidance): M11 SessionPlayer's `pendingRecord_` lifecycle or
M10 SessionWriter's QThread wakeup. Both are `.cpp`-only
fixable, but per spec §2.2 #1 ("No new code in V1 module
surfaces") any fix in M13 requires explicit ADR-008 +
authorization.

---

## C3 — Combined hardware-verification protocol authorship

### Statement

Spec §4.5 calls for a **combined 18-test** HW verification
protocol = M9's 6 + M10's 6 + M11's 6. Each prior protocol
exists at `docs/m9-hardware-verification.md`,
`docs/m10-hardware-verification.md`,
`docs/m11-hardware-verification.md`. Those files are
merged-to-main artefacts of prior milestones.

CC may NOT modify the prior protocol files (they are part of
the M9/M10/M11 closures). The consolidation is a **new**
file referencing or quoting the originals.

### Resolution

`docs/m13-hardware-verification.md` (S3 deliverable):
- Preamble explaining the V1.0 dogfood-session purpose
- Setup instructions (Ubuntu 24.04 host, mock connections via
  socat / python http.server, sample `.sfreplay` fixture)
- **Quotes** each prior protocol verbatim under section
  headers `## M9 — Connection Manager (6 tests)`,
  `## M10 — Session Writer (6 tests)`, `## M11 — Replay UX
  (6 tests)`. Verbatim quotes mean the operator has
  everything they need offline (the .deb ships docs).
- Acceptance bar from spec §5.2: **16/18** pass for V1.0 ship.
- Per-test result-recording table for the operator to fill in
  during the session.

No modification of M9/M10/M11 protocol files. M13 references
them by section name + quotes content.

---

## C4 — DEB install verification can't run in CI / on dev host

### Statement

Spec §4.6 lists 11 install-verification steps requiring a
**clean** Ubuntu 24.04 host. CC's dev host has the entire
build environment (Qt 6.10, all deps, prior install artefacts).
Running `sudo dpkg -i ...` on it would not represent a real
user's first install.

### Resolution

Two-tier verification:

**Tier 1 — CC-runnable structural validation** (S5):
- `tests/integration/test_m13_deb_package.cpp`: builds the
  `.deb`, runs `dpkg-deb --info` + `dpkg-deb --contents`,
  asserts package metadata (name, version, deps, install
  size) + file list match spec §2.1-3 expectations.
- `tests/integration/test_m13_release_artifacts.cpp`:
  asserts that the source-tree files that should land in the
  `.deb` actually exist before the package step (e.g.,
  `installer/signalforge.desktop`, the doc files, etc.).
- These are CI-runnable; ctest captures them.

**Tier 2 — Operator-run install verification** (S6):
- M13-done.md hand-off contains the 11-step protocol verbatim
  (mirroring spec §4.6).
- Operator runs on a clean VM / container (Ubuntu 24.04
  Vagrant box, Multipass instance, or fresh Docker container
  with Qt 6.10 installed). Records pass/fail per step in
  M13-done.md.
- Acceptance: 11/11 steps pass (per spec §3.5 V).

Tier 1 is enough to claim the `.deb` is structurally correct.
Tier 2 is required for V1.0 ship per spec §3.5 V. M13-done.md
records both tiers; operator-pending status is honest until
Tier 2 is run.

---

## C5 — `v1.0.0` tag finality

### Statement

Spec §6.1: "Once `v1.0.0` is tagged, V1.0 binary contents are
immutable. V1.0.1 patch releases would require new tag + new
ADR."

This is the strongest freeze in V1 development. Once `v1.0.0`
is pushed to origin, the artefact at that commit is the V1.0
release; no retag, no overwrite.

### Resolution

Pre-tag triple-check sequence at S6 close + Phase 3 gate:

S6 (this milestone):
- M13-done.md PR + merge state section captures the **exact
  merge SHA** that `v1.0.0` will point at (filled after PR
  creation).
- Acceptance §8 self-check ensures all 4 release prerequisites
  are met (or marked operator-pending) BEFORE Phase 1 step 6
  announce.

Phase 3 (next session, after operator confirms HW + DEB
install pass):
- Authorized git operations sequence per plan §S7:
  1. `gh pr merge <PR> --merge --delete-branch=false`
  2. `git tag -a v1.0.0 <MERGE-SHA> -m "SignalForge v1.0.0"`
  3. `git push origin v1.0.0`
  4. `gh release create v1.0.0 --notes-file
     docs/release-notes/v1.0.0.md signalforge_1.0.0_amd64.deb`
- Each is per-operation authorized in the next session's prompt.

The `gh release create` step is the **V1.0 ship moment** —
irreversible. CC will pre-flight check before invoking:
- Merge SHA matches what M13-done.md recorded
- Release notes path resolves
- `.deb` artefact path resolves and `dpkg-deb --info` passes
- All 4 release prerequisites confirmed met by operator

Any pre-flight failure → HALT, do not invoke `gh release
create`.

---

## C6 — V1.0 spec-list freeze sha256 collection

### Statement

Spec §6.3 + §4.7 + §8.5: `docs/v1.0-spec-list.md` must
contain every M2-M11 frozen `.hpp` sha256. CC must collect
these from the on-disk files at M13 close — **not** from
prior done.md files (those are time-of-write snapshots and
may differ if anything drifted).

### Resolution

S6 sha256 collection script:
1. Curated frozen-`.hpp` list (gathered from M2-M11
   `.claude/M*-done.md` §Freezes sections).
2. Run `sha256sum` on each path; capture into a table in
   `docs/v1.0-spec-list.md`.
3. **Cross-check** each computed sha256 against the
   prior done.md value:
   - Match → ✅ recorded as "frozen since M<n> close"
   - Mismatch → **HALT (H1 trigger)**: indicates post-freeze
     drift somewhere. Investigate which commit changed the
     file (`git log -p <file>`). HALT until human input.

This automates the V1 governance audit — ensures the V1.0
freeze record is accurate to the byte at tag time.

The current frozen-`.hpp` list (curated from M2-M11
done.md §Freezes):

| Module | File | Recorded at |
|---|---|---|
| M2 | (driver/buffer/decode interfaces) | M2 close |
| M3 | `src/drivers/driver_interface.hpp` (and concrete drivers) | M3 close |
| M4 | `src/pipeline/frame_pipeline.hpp` | M4 close |
| M5 | `src/decode/decoder_interface.hpp`, `decoder_registrar.hpp`, `schema_decoder.hpp` | M5 close |
| M6 | `src/buffer/signal_buffer.hpp`, `signal_buffer_registry.hpp` | M6 close |
| M7 | `src/expression/expression_engine.hpp` | M7 close |
| M8 | `src/chart/chart.hpp`, `chart_manager.hpp`, `chart_view.hpp`, `signal_selector.hpp`, `time_axis_manager.hpp` | M8 close |
| M9 | `src/connection/connection.hpp`, `connection_manager.hpp` | M9 close |
| M10 | `src/session/session_writer.hpp`, `session_metadata.hpp` | M10 close |
| M11 | `src/replay/playback_controller.hpp`, `session_player.hpp`, `replay_mode_manager.hpp` | M11 close |

S6 will canonicalise this list against the actual on-disk
state. Any file that should be frozen but isn't on disk =
HALT. Any file on disk whose content drifted from the M11
closure sha256 = HALT.

---

## Summary

| ID | Resolution path | ADR? | Ships in subtask |
|---|---|---|---|
| C1 | Two-class deliverable: CC ships infrastructure; operator runs HW + clean-VM install + Release publish | No | S0-S6 (CC) + S7 Phase 3 (operator) |
| C2 | Sequential 30-min soaks; backgrounded for parallel doc work; HALT on > 10 % growth | No | S4 |
| C3 | Combined HW protocol quotes M9/M10/M11 verbatim with V1.0 dogfood preamble | No | S3 |
| C4 | Tier 1 (CC structural) + Tier 2 (operator clean-VM install) | No | S5 + S6 |
| C5 | Pre-tag triple-check at S6 + Phase 3 pre-flight | No | S6 + S7 (Phase 3) |
| C6 | Automated sha256 collection at S6; mismatch = HALT (H1) | No (default) | S6 |

**No ADR-008 expected for V1.** This file is the canonical
record; M13-done.md will reference it from §Deviations.
