# M13 — Understanding

Source of truth: `docs/milestones/M13-packaging.md` (620 lines,
merged to `main` at `a25056b` via PR #23). Architectural
prereqs: all M0-M12 freezes + M9/M10/M11 hardware-verification
protocols (in `docs/`) + M5-M12 baseline.md files (in
`tests/benchmark/results/`) + M10/M11 soak harnesses
(`bench_session_writer --soak`, `bench_replay --memory-soak`).

This is the **final V1 milestone** — no successor milestone.
Closure produces `v1.0.0` tag + GitHub Release.

---

## 1. Goal in one paragraph

M13 ships V1.0 as an Ubuntu 24.04 amd64 `.deb` package
(`signalforge_1.0.0_amd64.deb`). It records the V1 freeze
surface (every M2-M12 frozen `.hpp` sha256 + 7 ADRs + format
specs), authors the user-facing release / install / spec
documentation, and gates the release on **4 blocking
prerequisites** accumulated from prior milestones: 30-min M10
memory soak, 30-min M11 memory soak, 18-test combined
hardware-verification dogfood session (M9 + M10 + M11), and
DEB install verification on clean Ubuntu 24.04. After M13,
V1.0 is locked: no further frozen-interface changes; future
work is V1.5+ (additive) or V2 (breaking). M13 is **packaging-
only** — no new V1 features, no frozen-`.hpp` modifications.

## 2. What ships (per spec §2.1)

1. CMake CPack configuration (`cmake/cpack-deb.cmake`) +
   wired into the root `CMakeLists.txt`. Targets Ubuntu 24.04
   amd64. Install path `/opt/signalforge/`; symlinks at
   `/usr/local/bin/{signalforge,sfreplay_inspect}`.
2. `signalforge_1.0.0_amd64.deb` build artefact via
   `cmake --build build/release --target package`. Includes
   the GUI binary, `sfreplay_inspect`, `profile_main`, V1
   docs, M5-M12 baseline.md, profile harness, and a
   `.desktop` launcher entry.
3. Post-install + pre-removal scripts (`cmake/deb-scripts/`)
   for MIME registration, icon-cache update, desktop-database
   update, and user-config preservation on removal.
4. Documentation:
   - `docs/release-notes/v1.0.0.md`
   - `docs/install.md`
   - `docs/v1.0-spec-list.md` (V1.0 freeze record — every
     frozen `.hpp` sha256, 7 ADRs, format specs)
   - `docs/m13-hardware-verification.md` (combined 18-test
     protocol = M9's 6 + M10's 6 + M11's 6)
5. **Release prerequisites** (blocking per spec §3.5 V):
   - M10 30-min memory soak via `bench_session_writer --soak 1800`
   - M11 30-min memory soak via `bench_replay --memory-soak 1800`
   - 18-test combined hardware verification — operator-run
     (real Serial/TCP/UDP/Replay connections + GUI session)
   - DEB install verification on clean Ubuntu 24.04 —
     operator-run (no clean VM available in CC's dev host)
6. M13 integration tests:
   - `tests/integration/test_m13_deb_package.cpp` —
     CI-runnable validation that `.deb` structure matches
     spec (file paths, control fields)
   - `tests/integration/test_m13_release_artifacts.cpp` —
     validates required files present + binaries launchable
7. Phase 3 closure (next session): `v1.0.0` tag + GitHub
   Release with `.deb` artefact + release-notes body.
8. `.claude/M13-done.md` + V1.0 freeze verification.

## 3. Out of scope (per spec §2.2)

- New V1 features (M11 + M12 surface = V1.0 release surface).
- M2-M12 frozen-`.hpp` modifications (M13 is packaging-only;
  any code change forces ADR-008 + spec-review cycle).
- Multi-platform (macOS / Windows) — V1.5+.
- Code signing / notarisation — V1.5+.
- App store / repository submission (PPA, Flathub) —
  V1.5+ infra; V1.0 is GitHub Releases only.
- New top-level dependencies beyond M0-M12.
- Automated release CI — V1.0 release is manual.
- Static linking of Qt — V1.5+ if needed; V1.0 uses system Qt 6.10.
- Localization / i18n — V1.5+.

## 4. Locked design decisions (spec §3)

| ID | Decision | Implication |
|---|---|---|
| M13.1 B | Linux .deb only (Ubuntu 24.04 amd64) | One distro target; source build documented for others |
| M13.2 Y | Comprehensive package contents (binary + tools + docs + baselines) | `/opt/signalforge/` layout; offline docs |
| M13.3 P | Traditional `v1.0.0` versioning | Final tag; V1.5+ → `v1.5.0`, V2 → `v2.0.0` |
| M13.4 S | GitHub Releases + `/opt/` install | `.deb` hosted on GitHub release page |
| M13.5 V | Release prerequisites blocking | 4 gates (2 soaks + 18 HW tests + DEB install); 16/18 acceptable for HW |
| M13.6 | No soft-HALT (inherits M2-M12) | Standard CLAUDE.md HALT discipline |
| M13.7 | V1.0 freeze record format = `docs/v1.0-spec-list.md` | Canonical V1 record; V1.5+ amendments cross-reference this |

## 5. Concerns surfaced (recorded in `.claude/M13-concerns.md`)

### C1 — Two-class deliverable structure (CC vs operator)

M13's deliverable list mixes **CC-blocking** work (CMake,
scripts, docs, integration tests, soak runs) with **operator-
blocking** work (18-test hardware verification with real
connections, DEB install on a clean Ubuntu 24.04 VM, GitHub
Release publishing under operator's GitHub account).

CC cannot:
- Plug in real Serial / TCP / UDP devices
- Run a clean Ubuntu 24.04 VM and `dpkg -i` the .deb on it
- Author user GitHub credentials for the Release workflow

CC can:
- Author the protocols / scripts / docs the operator runs
- Run the 30-min soaks on the dev host (background tasks)
- Build the `.deb` locally and run integration tests against
  its **structure** (e.g., `dpkg-deb --info`, file presence)

**Resolution**: plan §S0-§S5 are CC-blocking; §S6 (operator
verification) is **partially CC-doable** (CC can build the
`.deb` and verify its structure; the install-on-clean-Ubuntu
step is operator-run with a CC-authored protocol).
M13-done.md will record both classes and explicitly mark
operator-pending items.

### C2 — Soak runs in M13 timeline

M10 + M11 each require a 30-min memory soak. Running them
**sequentially** = 60 min wall-clock; **in parallel** =
30 min but with potential thermal / CPU contention skewing
results. The user's earlier guidance ("These are NOT separate
follow-ups; they ARE M13 acceptance criteria") commits CC
to running both during M13.

**Resolution**: run sequentially (cleaner data; no
contention). Use `run_in_background=true` so CC can continue
authoring docs while a soak completes. Total CC wall-time
overhead: ~60 min spread across S4. Acceptable.

If a soak FAILS (> 10 % VmRSS growth), HALT per spec §7-5 +
file ADR-008 (or hotfix ADR — would be V1.0.1 territory if
discovered post-tag, but for M13 it's a release blocker
requiring `.cpp`-only fix or feature delay).

### C3 — Combined hardware-verification protocol authorship

The 18-test combined protocol (`docs/m13-hardware-verification.md`)
is a *consolidation* of three existing protocols (M9: 6
tests, M10: 6 tests, M11: 6 tests). CC authors the consolidated
doc; the operator executes the 18 tests in a single dogfood
session.

CC may NOT modify the existing M9/M10/M11 protocol files —
those are merged-to-main artefacts of the prior milestones.
The consolidation is a NEW file that **references** the
originals (or quotes them in full for offline use within the
.deb).

**Resolution**: `docs/m13-hardware-verification.md` quotes
each prior protocol verbatim with section headers, adds a
"V1.0 dogfood session" preamble, and notes the 16/18
acceptance bar from spec §5.2.

### C4 — DEB install verification can't run in CI

Spec §4.6 calls for a 11-step DEB install verification on
clean Ubuntu 24.04. CC's dev host is not "clean" — it has the
build environment, all dependencies, and prior install
artefacts. Running `sudo dpkg -i ...` on it would not be
representative of a real user's first install.

**Resolution**: CC builds the `.deb` and runs CI-friendly
structural checks (`dpkg-deb --info`, `--contents`, file
presence, control-field verification) in the integration
test. The 11-step end-to-end install on a clean VM is
operator-run with a CC-authored protocol document.
M13-done.md records this split explicitly.

### C5 — `v1.0.0` tag finality

Spec §9: "Once `v1.0.0` is tagged, V1.0 binary contents are
immutable. V1.0.1 patch releases would require new tag + new
ADR." This is the strongest freeze in V1 — once shipped, the
tag is unmovable.

**Resolution**: pre-tag triple-check at S6 close + Phase 3
gate. M13-done.md's PR + merge state section captures the
exact merge SHA the v1.0.0 tag will point at. The Phase 3
operations sequence will be clearly documented for the next
session (operator-driven).

### C6 — V1.0 spec-list freeze sha256 collection

Spec §6.3 + §4.7 + §8.5: `docs/v1.0-spec-list.md` must
contain every M2-M11 frozen `.hpp` sha256. CC must collect
these from the on-disk files at M13 close — not from prior
done.md files (which may have been written when the file was
in slightly different state). The collection script is a
straightforward `sha256sum` invocation.

**Resolution**: at S3, CC runs `sha256sum src/**/*.hpp` for
the canonical freeze list (curated via M2-M11 done.md
references), records each in `docs/v1.0-spec-list.md`, and
verifies each matches the corresponding done.md record.
Mismatch = HALT (would indicate post-freeze modification).

## 6. Integration surfaces I will touch

| File / area | Status | Why I touch it |
|---|---|---|
| `CMakeLists.txt` | top-level | Bump version 0.0.1 → 1.0.0, add `install()` rules + `include(cmake/cpack-deb.cmake)` |
| `cmake/cpack-deb.cmake` (new) | new | CPack config + `CPACK_DEBIAN_*` |
| `cmake/deb-scripts/postinst` (new) | new | MIME / icon / desktop database update |
| `cmake/deb-scripts/prerm` (new) | new | User-config preservation message |
| `installer/signalforge.desktop` (new) | new | Linux launcher entry |
| `installer/signalforge.png` (new) | new | App icon (256×256 placeholder for V1.0) |
| `docs/release-notes/v1.0.0.md` (new) | new | User-facing release notes |
| `docs/install.md` (new) | new | Install + troubleshooting guide |
| `docs/v1.0-spec-list.md` (new) | new | V1.0 freeze record |
| `docs/m13-hardware-verification.md` (new) | new | Combined 18-test protocol |
| `tests/integration/test_m13_*.cpp` (new) | new | CI-runnable .deb validation |
| `tests/benchmark/results/M13-final-soak.md` (new) | new | Soak run results |
| `.claude/M13-{concerns,progress,done}.md` (new) | new | Governance |

## 7. Definition of done (per spec §8 + CLAUDE.md)

A task is done when **all** of these hold:

1. Code (CMake) compiles cleanly under Debug + Release +
   debug-asan; `cmake --build build/release --target package`
   produces a valid `.deb`.
2. ctest green on Debug + Release; debug-asan green on CI.
3. M2-M12 freeze sha256s verified intact (no `.hpp`
   modified during M13).
4. **4 release prerequisites met**:
   - M10 30-min soak < 10 % VmRSS growth ✓
   - M11 30-min soak < 10 % VmRSS growth ✓
   - 16/18 hardware tests pass (operator-run) ✓
   - DEB install verification 11/11 steps (operator-run) ✓
5. Documentation suite published (release notes, install,
   spec list, hardware verification protocol).
6. M13-progress.md tracks every subtask with build / test
   counts and operator-pending items.
7. PR opened to `main`, CI green, awaiting Phase 2 approval.
8. After merge (Phase 3 next session): `v1.0.0` tag pushed,
   GitHub Release published with `.deb` artefact.

## 8. Effort sketch

Spec target: 3-5 person-days. M13 is documentation-heavy +
infrastructure (CMake CPack) + operator-run gates. CC's
work-time is dominated by:

- S1 CMake CPack: ~150 LOC CMake + 50 LOC scripts
- S2 desktop entry + icon placeholder: ~30 LOC
- S3 documentation: ~600 LOC across 4 files
- S4 soaks: ~30 LOC results doc + ~60 min wall-clock
- S5 integration tests: ~150 LOC C++
- S6 done.md + freeze verification: ~400 LOC docs + sha256 harvest
- Phase 3 v1.0.0 tag + Release: operator + CC-authorized

Total CC-authored: ~1 500 LOC. Compares to M11's ~3 850 and
M12's ~2 100; M13 is lighter in code, heavier in process.

## 9. Phase 2 follow-ups inherited (now M13 acceptance)

These were tracked as separate items through M9-M12; M13
spec §3.5 V folds them into M13's acceptance:

1. M10 30-min memory soak ✓ closed in M13 S4
2. M11 30-min memory soak ✓ closed in M13 S4
3. 18-test combined hardware verification ✓ closed in
   M13 S5 (CC authors protocol; operator executes)
4. DEB install on clean Ubuntu 24.04 ✓ closed in M13 S6
   (operator-run with CC-authored protocol)

After M13 close, all four are V1.0-shipped — no longer
"follow-up" items.

## 10. Critical execution notes

- **No new code in V1 module surfaces.** M13 is packaging
  only. Any code-level finding during M13 (e.g., a soak
  reveals a leak) becomes an ADR + V1.0.1 patch decision —
  do NOT silently fix in M13.
- **Real install on real Ubuntu**: CC ships the `.deb` +
  authors the protocol; operator validates on a clean VM.
  Don't claim DEB install verification "passed" without
  operator confirmation.
- **The v1.0.0 tag is final**. Triple-check release notes,
  install instructions, sha256s, and the merge SHA before
  Phase 3 tag.
- **Soaks are ~30 min each** — plan timing accordingly. CC
  may run them in background while authoring docs.
- **Documentation is part of the release**. Make release
  notes + install.md clear, complete, copy-pasteable. Users
  won't read code; they read these docs.
