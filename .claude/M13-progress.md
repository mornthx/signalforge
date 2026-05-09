# M13 — Progress log

Per CLAUDE.md §Required #2 + plan §0, every subtask logs start +
close entries with build / test / format counts and any deviations.

---

## Pre-S0 — M13 understanding + plan (completed)

- Start: 2026-05-09T10:30Z
- Close: 2026-05-09T10:40Z
- Commits:
  - `de012a6` "chore: record M13 understanding and plan" (initial)
  - `080fe28` "merge: pull origin/main with M13 spec into milestone/M13" (corrective merge — milestone/M13 was created at `9094f74` before PR #23 landed; merge-from-main brought the spec onto the branch without violating §Forbidden #3)
- CI: pending — push triggers CI per CLAUDE.md §Required #2.
- Deliverables:
  - `.claude/M13-understanding.md` (293 lines, 6 concerns
    C1-C6 surfaced; two-class deliverable structure
    explicitly framed in §C1)
  - `.claude/M13-plan.md` (330 lines, S0-S6 sequenced + S7
    Phase-3-deferred; 7 HALT triggers H1-H7)

---

## S0 — M13-concerns.md (completed)

- Start: 2026-05-09T10:55Z

### Deliverables

- `.claude/M13-concerns.md` (~290 lines): canonical record of
  C1-C6 with subtask anchors + decision trees.
  - **C1** Two-class deliverable structure (CC vs
    operator-blocking) — explicit split table.
  - **C2** Soak-runs timeline — sequential, backgrounded,
    HALT path defined for > 10 % growth.
  - **C3** Combined HW protocol — quote M9/M10/M11
    verbatim; no modification of prior protocols.
  - **C4** DEB install — Tier 1 (CC structural) + Tier 2
    (operator clean-VM).
  - **C5** v1.0.0 tag finality — pre-tag triple-check at
    S6 + Phase 3 pre-flight.
  - **C6** sha256 collection at S6 — drift detection via
    cross-check against M2-M11 done.md records.
- No ADR-008 authored. Default position holds.

### Build / test counts

- Docs-only commit. CLAUDE.md §Required #2 exception applies
  (build graph unaffected).
- No regression / soak / integration cycle (per plan §0).

### Deviations from plan

- Plan §S0 anticipated ~250 LOC; actual ~290 LOC. The C6
  curated-frozen-`.hpp` table was longer than estimated
  because each module's freeze record needed listing for the
  S6 cross-check. No spec deviation.

S0 commit: `d7be620` "docs: M13 S0 — concerns C1-C6 (no
ADR-008)". Pushed; CI in flight.

---

## S1 — CMake CPack + install rules (completed)

- Start: 2026-05-09T11:05Z

### Deliverables

- `CMakeLists.txt`:
  - **Version bump**: `project(SignalForge VERSION 0.0.1)` →
    `VERSION 1.0.0` per spec §3.3 P (canonical V1 final
    version).
  - Append `include(cmake/install.cmake)` and
    `include(cmake/cpack-deb.cmake)` at end.
- `cmake/install.cmake` (~75 LOC): all `install()` rules per
  spec §2.1-3:
  - Binaries (`signalforge`, `sfreplay_inspect`,
    `profile_main`) → `bin/` (i.e., `/opt/signalforge/bin/`)
  - V1.0 user docs (install.md, v1.0-spec-list.md, M9/M10/M11
    + combined M13 hardware verification) → `docs/`
  - Release notes → `docs/release-notes/`
  - SFREPLAY v1 format spec → `docs/format/`
  - All ADR-* markdown files → `docs/architecture/decisions/`
    (via `install(DIRECTORY ... FILES_MATCHING)`)
  - M3-M12 baseline.md files → `benchmarks/results/`
  - Profile harness scripts → `tools/profile/`
  - Desktop entry → `/usr/share/applications/` (absolute)
  - 256×256 icon → `/usr/share/icons/hicolor/256x256/apps/`
    + legacy `/usr/share/pixmaps/` (absolute)
- `cmake/cpack-deb.cmake` (~50 LOC): CPack DEB config per
  spec §4.1:
  - `CMAKE_INSTALL_PREFIX = /opt/signalforge` (force)
  - `CPACK_GENERATOR = "DEB"`, package name `signalforge`,
    version from `${PROJECT_VERSION}` (= 1.0.0)
  - DEB control fields: maintainer, homepage, section
    `science`, priority `optional`, architecture `amd64`,
    file name `signalforge_1.0.0_amd64.deb`
  - Dependency manifest: `libc6 (>= 2.38)`, `libstdc++6
    (>= 13)`, `libyaml-cpp0.8 (>= 0.7)`. Qt 6.10 dep is
    documented in install.md (V1.0 user installs from
    external repo per spec §9 note).
  - `CPACK_STRIP_FILES = TRUE` (smaller package).
  - `CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA` points at S2's
    `cmake/deb-scripts/postinst` + `prerm`.
  - `CPACK_RESOURCE_FILE_LICENSE` → repo-root `LICENSE`.

### Build / test counts

- Debug + Release build clean (incremental — only the new
  CMakeLists changes ripple).
- Configure step parses cpack-deb.cmake correctly. CPack
  `package` target depends on S2 (postinst/prerm scripts).
- ctest unchanged (no test code changes in S1).
- ASan local blocked by host /etc/ld.so.preload; CI authoritative.

### Deviations from plan

- Plan §S1 anticipated `~250 LOC` total; actual ~125 LOC
  CMake (~75 install + ~50 cpack). The lower count reflects
  splitting `install.cmake` from `cpack-deb.cmake` for clarity
  and using `install(DIRECTORY ... FILES_MATCHING)` for ADRs
  (single line vs explicit listing). Within target.
- Spec §4.1's example `CPACK_DEBIAN_PACKAGE_DEPENDS` lists
  `qt6-base-dev` etc. as runtime deps — but `*-dev` are
  development packages, not runtime. M13 ships runtime
  packages only (`libc6`, `libstdc++6`, `libyaml-cpp0.8`).
  Qt 6.10 is documented in install.md (S3 deliverable) per
  spec §9 note ("V1.0 documents the requirement; user
  installs Qt 6.10 themselves").

S1 commit: `8c6c255` "build: M13 CMake CPack DEB config + install
rules (M13 S1)". Pushed; CI in flight.

---

## S2 — Post-install scripts + desktop entry + icon (completed)

- Start: 2026-05-09T11:25Z

### Deliverables

- `cmake/deb-scripts/postinst` (~50 LOC): bash post-install
  per spec §4.2:
  - Creates symlinks `/usr/local/bin/{signalforge,
    sfreplay_inspect}` → `/opt/signalforge/bin/...` (the
    spec §3.4 symlink layer; deferred from CMake `install()`
    to postinst per S1 §Deviations).
  - `update-mime-database /usr/share/mime/` (best-effort).
  - `gtk-update-icon-cache -f -t /usr/share/icons/hicolor/`
    (best-effort).
  - `update-desktop-database /usr/share/applications/`
    (best-effort).
  - User-facing welcome message with launch hints + dialout
    group note for serial-port access.
  - `set -e`; `bash -n` syntax-clean; `chmod +x` set.
- `cmake/deb-scripts/prerm` (~20 LOC): pre-removal per spec
  §4.3:
  - Removes the postinst-created symlinks idempotently.
  - User-config preservation message.
  - `bash -n` syntax-clean; `chmod +x` set.
- `installer/signalforge.desktop`: per spec §4.4 + standard
  XDG keys (`Categories=Development;Engineering;Science`,
  `MimeType=application/x-sfreplay`, `StartupWMClass`,
  `Keywords` for menu search).
- `installer/signalforge.png`: 256×256 placeholder icon
  (3.8 KB) generated via Python PIL — solid blue-grey
  background, 3 stylized signal-waveform strokes, "SF"
  label. V1.5+ may swap for designer art.

### Build / test counts

- Configure step parses cleanly with the new
  `CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA` paths.
- `cmake --build build/release --target package` — fails
  with expected errors: missing `docs/install.md`,
  `docs/release-notes/v1.0.0.md`, etc. **These land in S3.**
  S2's scope (scripts + icon + desktop entry) is verified
  via syntax checks + file presence.
- ctest unchanged.
- `clang-format` not applicable (no C++ changes).

### Deviations from plan

- Plan §S2 anticipated ~80 LOC; actual ~85 LOC (scripts
  ~70 + desktop entry ~15). Within target. Icon is binary
  (3.8 KB), not counted in LOC.
- Spec §4.2's example postinst doesn't mention symlinks —
  M13 adds them per spec §3.4 ("Symlinks: /usr/local/bin/
  signalforge, /usr/local/bin/sfreplay_inspect"). The S1
  decision to defer symlink creation from CMake `install()`
  to postinst keeps the .deb's file list clean (no broken
  symlinks at unpack time).
- Postinst includes a user-facing dialout-group note for
  serial-port access — beyond spec §4.2's example but
  mirrors the user-experience improvements documented in
  install.md (S3).

S2 commit: `f3fc467` "build: M13 deb postinst/prerm scripts +
desktop entry + placeholder icon (M13 S2)". Pushed; CI in flight.

---

## S3 — Documentation suite (completed)

- Start: 2026-05-09T11:30Z

### Deliverables (4 docs, 1 125 LOC total)

- `docs/release-notes/v1.0.0.md` (~233 lines): user-facing
  V1.0 release notes per spec §2.1-5 + §10.
  - V1 feature summary (live + record + replay) per
    M9/M10/M11 + M12 perf
  - System requirements (Ubuntu 24.04, Qt 6.10)
  - Quick start (5-step example)
  - Performance highlights table (V1.0 baselines)
  - Known limitations (V1.5+/V2 deferred items)
  - Roadmap section
  - License + acknowledgements + GitHub Issues link
- `docs/install.md` (~248 lines): per spec §2.1-6.
  - 3 install paths (.deb, source, .tar.gz fallback
    documented as "not provided for V1.0")
  - Qt 6.10 dependency external-repo guidance
  - Serial-port `dialout` group setup
  - Uninstall + user-config preservation
  - 5 troubleshooting sections (command not found,
    libQt6Core.so missing, /dev/ttyUSB perms,
    .sfreplay associations, app crash logs)
  - Where-files-go reference table
- `docs/v1.0-spec-list.md` (~211 lines): canonical V1.0
  freeze record per spec §6.3 + §4.7.
  - **All 26 frozen `.hpp` files listed with sha256s**
    (collected via sha256sum on M3-M11 frozen surface)
  - **All sha256s cross-checked against
    .claude/M*-done.md §Freezes records — 0 drift
    detected**. H1 trigger clear.
  - File format spec sha256 (SFREPLAY v1)
  - 7 ADRs cross-referenced (ADR-001 through ADR-007)
  - M0-M13 spec list
  - 7 V1 architectural invariants
  - M13 release-artefacts table
- `docs/m13-hardware-verification.md` (~433 lines):
  combined 18-test protocol per spec §4.5.
  - Pre-flight + result template
  - **Section M9 (6 tests)**: Serial / TCP / UDP / Replay
    drivers + edit-remove + auto-connect (quoted from
    `docs/m9-hardware-verification.md`)
  - **Section M10 (6 tests)**: GUI round-trip + restart
    persistence + quit-while-recording + mid-stream
    catalog + backpressure + disk-full (quoted from
    `docs/m10-hardware-verification.md`)
  - **Section M11 (6 tests)**: GUI open + play/pause +
    step ◀▶ + scrubber + speed combo + Live↔Replay dialogs
    (quoted from `docs/m11-hardware-verification.md`)
  - 16/18 acceptance bar surfaced; failure documentation
    template provided
- `cmake/cpack-deb.cmake`: fixed install-prefix variable
  (`CPACK_PACKAGING_INSTALL_PREFIX = /opt/signalforge`)
  after first .deb build placed files at `/usr/...`.

### Build / test counts

- Configure clean.
- **`.deb` package builds successfully end-to-end** via
  `cmake --build build/release --target package`:
  - File: `build/release/signalforge_1.0.0_amd64.deb`
  - Size: **2.78 MB** (vs spec §5.1 < 50 MB target — 17×
    headroom)
  - Installed-Size: 8 022 KB
  - Architecture: amd64
  - Depends: libc6 (>= 2.38), libstdc++6 (>= 13),
    libyaml-cpp0.8 (>= 0.7)
- `dpkg-deb --info` validates control fields.
- `dpkg-deb --contents` confirms expected file layout:
  - `/opt/signalforge/bin/{signalforge,sfreplay_inspect,profile_main,crashpad_handler}` ✅
  - `/opt/signalforge/docs/...` (V1 docs + ADRs + format spec) ✅
  - `/opt/signalforge/benchmarks/results/M3-M12-baseline.md` ✅
  - `/opt/signalforge/tools/profile/...` ✅
  - `/usr/share/applications/signalforge.desktop` ✅
  - `/usr/share/icons/hicolor/256x256/apps/signalforge.png` + `/usr/share/pixmaps/...` ✅
  - postinst + prerm in package control area ✅
- ctest unchanged (no test code change).

### Sha256 cross-check (S6 deliverable folded forward)

Per concerns C6 + plan §S6, the V1.0 freeze sha256 collection
has been done at S3 (one less thing for S6). All 26 frozen
`.hpp` files match their M2-M11 done.md records exactly. **0
post-freeze drift.** H1 trigger clear.

The full table is in `docs/v1.0-spec-list.md §1`. S6 will
re-run the verification as part of the M13 close gate; given
the S0-S5 commits don't touch any frozen `.hpp`, no drift
is expected.

### Deviations from plan

- Plan §S3 anticipated ~700 LOC docs; actual ~1 125 LOC.
  Higher because the combined HW protocol (m13-hardware-
  verification.md, 433 lines) quotes M9 + M10 + M11
  protocols verbatim — necessary for offline use per spec
  §3.4 ("docs in install dir: works offline").
- Plan §S3 deferred sha256 collection to S6; S3 did it
  inline. S6 will verify it's still consistent at close time.
- The first .deb build placed files at `/usr/...` because
  CPack DEB uses `CPACK_PACKAGING_INSTALL_PREFIX` (not
  `CMAKE_INSTALL_PREFIX`) for its package root. Caught by
  the post-build `dpkg-deb --contents` check; cpack-deb.cmake
  amended; rebuild verified `/opt/signalforge/...` paths.

S3 commit: `c0f36f1` "docs: M13 V1.0 release notes + install +
spec list + 18-test HW protocol (M13 S3)". Pushed; CI in flight.

---

## S4 — 30-min memory soaks (HALT — H5 fired on M10 soak)

- Start: 2026-05-09T11:50Z
- Halted: 2026-05-09T12:15Z (after M10 soak ran ~270 s)

### M10 soak result

`bench_session_writer --soak 1800 --memory-snapshot 30`
exhibited **linear, unbounded VmRSS growth**: ~14 MB per
30-second snapshot (~470 KB/s sustained). At sec=270, VmRSS
was 138 420 KB (vs initial 25 724 KB) — **+438 % in 240 s**.

Projected 30-min final = ~870 MB → **~3 300 % growth** vs
the spec §5.3 HALT bar of `> 15 %`. **Two orders of magnitude
past HALT threshold.**

Background soak killed at sec≈270 once the linear pattern
was unambiguous. 9 snapshots preserved at
`tests/benchmark/results/M13-soak-data/m10-soak.jsonl`.

### Suspicious observations

- `bytes_written = 0` throughout the soak, despite 60 k
  events/sec sustained input + `dropped = 0`. The bench
  reads `writer.bytesWritten()` at line 144 of
  `tests/benchmark/bench_session_writer.cpp`; a zero-byte
  reading + sustained input + linear VmRSS growth is
  consistent with **events accumulating in memory rather
  than being flushed to disk**.
- This may be a bench-fixture-side artefact (e.g.,
  `bytesWritten()` only updates after `stop()`) rather
  than a real M10 SessionWriter leak. **CC has not
  investigated further** per the user's S4 failure-handling
  guidance.

### M11 soak — NOT STARTED

Per concerns C2 + the user's S4 failure-handling guidance,
CC stopped at the M10 H5 fire and did not proceed to the
M11 soak. M11 soak status is `operator-pending`.

### HALT report

Full H5 disposition + decision options (A/B/C/D) in
`.claude/halt/HALT-2026-05-09T12-15Z-m10-soak-leak.md`.

### Status

S4 was halted at 12:15Z; user authorized Option D
investigation at 12:25Z.

### S4 H5 root-cause investigation (Option D, completed)

User authorized investigation of the bench fixture vs
production code (Option D from the HALT report).

**Two bench-fixture bugs identified, neither a real M10
leak:**

1. **`bytes_written = 0` reading**: `SessionWriter::bytesWritten()`
   (line 166-167 of `session_writer.cpp`) reads from a
   cached atomic that's **only updated inside `stop()`**
   (line 128-129). During recording it always returns 0.
   The actual byte counter on `SessionFileWriter::bytesWritten_`
   IS incrementing correctly — the worker is writing.
2. **VmRSS growth**: bench's own `enqueueLatNs` vector
   `reserve()`d for `durationSeconds * 60 000` entries —
   at 30 min × 60 k events/sec × 8 bytes = **864 MB** of
   latency samples. The 470 KB/s growth pattern matched
   push-rate × sample-size exactly.

**Disk-write evidence**: on-disk fixture
`/tmp/bench_session_writer-LiFJtL/bench.sfreplay` was
**499 MB** at the killed soak. Worker IS writing at
~1.85 MB/s (60 k events × 28 bytes/event = exactly the
expected rate). M10 SessionWriter is **not leaking**.

### Bench fix applied (per user "authorized to fix bench")

`tests/benchmark/bench_session_writer.cpp`: replaced the
unbounded `enqueueLatNs` vector with a 100 000-entry
rolling buffer (~1.7 s window at 60 k events/sec).
Preserves the p99-statistic representativeness while
keeping bench memory bounded.

`bytes_written = 0` during recording is a known
limitation of the cached value path; not fixed in M13
(would touch the M10 frozen `SessionWriter` header → ADR-008
required). Bench output is informational only; the
worker-side counter is correct.

### Verification soak (3 min) post-fix

Re-ran with `--soak 180 --memory-snapshot 30`:

| sec | VmRSS (KB) | events_recorded |
|---:|---:|---:|
| 30 | 12 560 | 1 800 060 |
| 60 | 12 624 | 3 600 060 |
| 90 | 12 624 | 5 400 120 |
| 120 | 12 624 | 7 200 180 |
| 150 | 12 624 | 9 000 240 |

Final summary: vmrss_initial 11 716 KB → vmrss_final
12 812 KB → **growth 1.489 %** (vs spec §5.3 target < 10 %,
HALT > 15 %). 302 MB written to disk; 60 000 events/sec
sustained; 0 dropped.

**M10 soak now passes the spec acceptance**. H5 cleared
upon resolution.

### Final 30-min M10 soak — ✅ PASS

`bench_session_writer --soak 1800 --memory-snapshot 60`
completed normally.

| Metric | Result |
|---|---:|
| Duration | 1 800 s (30 min) |
| events_recorded | 108 000 000 |
| events_per_sec | 60 000 (sustained, matches M10 baseline) |
| bytes_written (final, after stop()) | 3 024 002 331 (3.0 GB) |
| dropped_events | 0 |
| enqueue_p99 | 11.87 µs |
| VmRSS initial | 11 728 KB |
| VmRSS baseline (sec 120+) | 12 636 KB |
| VmRSS final | 12 824 KB |
| **VmRSS growth** | **1.488 %** |

Spec §5.3 acceptance: < 10 % target (PASS); HALT > 15 %
(clear). H5 trigger from S4 first attempt: **resolved as
bench-fixture artefact; not a real M10 leak.**

### Final 30-min M11 soak — ✅ PASS (externally-verified)

`bench_replay --memory-soak 1800 --memory-snapshot 60`:
- The bench has a separate output-cosmetic bug
  (snapshot JSON lines never flush during runtime). Both
  soak attempts ran 42-44 min instead of 30 min wall-clock
  (per-loop seek+replay cost > nominal). VmRSS observed
  externally via `/proc/<pid>/status` polling.

| Metric | Result |
|---|---:|
| Run-1 duration | 44 min |
| Run-1 VmRSS (every check) | 36 152 KB (rock-stable) |
| Run-1 growth | **0 %** |
| Run-2 duration | 42 min |
| Run-2 VmRSS (every check) | 35 008 KB (rock-stable) |
| Run-2 growth | **0 %** |

Spec §5.3 acceptance: < 10 % target (PASS); HALT > 15 %
(clear). The M11 SessionPlayer + PlaybackController +
SessionReader pipeline is leak-free.

The bench's snapshot-flush cosmetic issue is documented as
a V1.0.1 candidate in `M13-done.md §What's deferred` (not
a release blocker — the underlying behaviour we care about
is observable externally).

### S4 status — both gates closed

| Soak gate | Result | Evidence |
|---|---|---|
| M10 30-min memory soak | ✅ 1.488 % growth | `tests/benchmark/results/M13-soak-data/m10-soak-final.jsonl` |
| M11 30-min memory soak | ✅ 0 % growth (externally observed) | `tests/benchmark/results/M13-final-soak.md §2` |

S4 commits:
- `514d4ab` "bench: rolling-buffer enqueueLatNs — M13 S4
  H5 root cause was bench leak, not M10" (M10 bench fix)
- (this commit at S6 close): final-soak.md + done.md +
  M13 closure

---

## S6 — Final M13 baseline + done.md + freeze verification (completed)

- Start: 2026-05-09T11:35Z (in parallel with M10 30-min soak)
- Close: 2026-05-09T13:35Z

### Deliverables

- `.claude/M13-done.md` (~245 lines): completion report
  per spec §6.3 + plan §S6:
  - Spec §2.1 deliverables checklist (14 items: 11 ✅,
    3 marked operator-pending or Phase-3-deferred)
  - PR + merge state placeholders (filled at PR creation)
  - **V1.0 freeze record** section: 26 frozen `.hpp`
    sha256s + format spec + 6 ADRs (numbering jumps
    005 → 007). 0 drift detected.
  - Acceptance self-check vs spec §8 (8.1-8.7)
  - **Release prerequisite status table**: 2 of 4
    closed (M10 soak ✅, M11 soak ✅);
    2 operator-pending (HW verification, DEB install
    on clean Ubuntu)
  - HALT trigger disposition: H5 fired and resolved
  - Concerns C1-C6 summary
  - Commit manifest (Pre-S0 through S6)
  - V1.0.1 / V1.5+ / V2 deferred items
  - Hand-off section (V1 ships)
  - Phase 3 sequence (next session)
- `tests/benchmark/results/M13-final-soak.md` (~150
  lines): consolidated soak results doc.
- `tests/benchmark/bench_replay.cpp`: minor fflush
  additions (cosmetic; the deeper snapshot-emit issue
  remains as a V1.0.1 candidate).

### Frozen surface re-verification at S6

sha256 cross-check at S6 close vs `docs/v1.0-spec-list.md`
(authored at S3): all 26 frozen `.hpp` files match exactly.
**No drift between S3 and S6.** S6 confirms the V1.0 freeze
record is byte-accurate at the M13 close.

H1 trigger (frozen `.hpp` modification): **clear**.

### Build / test counts

- Debug + Release + debug-asan all build clean.
- ctest: Debug **602/602** + Release **602/602** unchanged
  from S5 close (S6 was docs + bench-cpp tweaks only).
- `cmake --build build/release --target package` succeeds;
  produces `signalforge_1.0.0_amd64.deb` (2.78 MB).
- 8 of the 8 `test_m13_deb_package` integration cases pass
  against the just-built `.deb`.
- ASan local blocked by host /etc/ld.so.preload; CI authoritative.

### Deviations from plan

- Plan §S6 anticipated ~500 LOC; actual ~395 LOC done.md
  + 150 LOC final-soak doc + 4 LOC bench fflush = ~550
  LOC. Within target.
- Plan §S6 sha256 collection at S6: pre-collected at S3
  per the early-collection optimisation; S6 re-verifies
  at close (0 drift).
- The M11 soak result is documented via external VmRSS
  observation rather than the bench's stdout (per the
  bench cosmetic noted in final-soak.md §2). The
  acceptance evidence (VmRSS rock-stable for 42 min) is
  unambiguous.

S6 commit: pending push.

---

## S5 — Integration tests (completed in parallel during S4)

- Start: 2026-05-09T12:00Z (during M10 soak)
- Close: 2026-05-09T12:10Z

### Deliverables

Authored + tested in parallel during the M10 soak per
concerns C2 background-doc-work plan. S5 is **independent
of S4**: integration tests don't depend on soak results.

- `tests/integration/test_m13_release_artifacts.cpp` (~85
  LOC): 7 cases verifying source-tree files exist:
  - V1.0 release docs (release-notes, install, spec-list,
    HW protocol)
  - Prior milestone HW verification protocols (M9, M10, M11)
  - Frozen format spec (sfreplay-v1)
  - 6 ADRs (numbering jumps 005 → 007; ADR-006 was skipped
    at M7 close — discovery during S5; updated
    `docs/v1.0-spec-list.md` accordingly)
  - M3-M12 baseline.md
  - DEB scripts + desktop entry + icon
  - `LICENSE` (referenced by CPack)
- `tests/integration/test_m13_deb_package.cpp` (~125 LOC):
  8 cases via `dpkg-deb`. Each test SKIPs cleanly if the
  `.deb` isn't pre-built (CI doesn't build `package` target):
  - .deb file present
  - `dpkg-deb --info` control fields (name / version /
    arch / section / priority / depends)
  - `/opt/signalforge/bin/*` binaries
  - V1.0 docs + benchmarks
  - Desktop entry + icons
  - Profile harness scripts
  - ADRs (6 total — ADR-006 skipped per source tree)
  - File size < 50 MB (spec §5.1 target)
- `tests/integration/CMakeLists.txt`: appends both targets
  with `SIGNALFORGE_SOURCE_ROOT` compile-define for path
  resolution.

### Build / test counts

- Debug + Release build clean.
- ctest: Debug **602/602** + Release **602/602** (+15 from
  M12 close at 587):
  - test_m13_release_artifacts: 7 cases / 32 assertions
  - test_m13_deb_package: 8 cases / 30 assertions
- All M0-M12 regression detectors quiet.

### Discovery during S5 — ADR-006 missing

S5's "ADRs all present" test surfaced a discrepancy:
`docs/architecture/decisions/` contains only **6** ADRs
(numbering 001-005 + 007), not 7 as
`docs/v1.0-spec-list.md` originally claimed. ADR-006 was
reserved for "M7 cycle detection in expression engine"
during early planning but was not authored at M7 close;
the M7 cycle-detection design lives in
`.claude/M7-done.md` instead.

`docs/v1.0-spec-list.md §3` updated at S5 to reflect
reality (6 ADRs, numbering jump explained). Test file
references the correct 6 ADR filenames.

This is a **documentation correction**, not a freeze
violation — no .hpp / format spec changed. H1 trigger
remains clear.

### S5 commit

S5 commit pending push (committed alongside S4 HALT report
to preserve the milestone state on origin per CLAUDE.md
HALT protocol "commit whatever is committable").
