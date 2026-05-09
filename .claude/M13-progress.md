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

S3 commit: pending push.
