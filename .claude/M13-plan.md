# M13 — Plan

Pairs with `.claude/M13-understanding.md`. Source of truth:
`docs/milestones/M13-packaging.md` at `a25056b`. CLAUDE.md
governs.

This is the **final V1 milestone**. Closure produces `v1.0.0`
tag + GitHub Release.

---

## 0. Methodology

- **Packaging-only**: no new code on M0-M12 surfaces. CMake +
  scripts + docs + integration tests only. Any code-level
  finding during M13 (e.g., a soak reveals a leak) becomes an
  ADR + V1.0.1 decision; do NOT fix silently.
- One subtask = one logical commit (CLAUDE.md §Required #3).
- Build gate before every `.deb`-affecting commit:
  Debug + Release + debug-asan build clean; ctest green;
  `cmake --build build/release --target package` succeeds;
  `dpkg-deb --info` validates control fields.
- Documentation-only commits use the CLAUDE.md §Required #2
  exception (S0 / S2 / S3 / S6 / S7 doc commits qualify).
- HALT triggers in §3 fire **immediately**.
- Release prerequisites in §3.5 V are **blocking**: 4 gates
  (2 soaks + 18 HW tests + DEB install). 16/18 acceptable
  for HW.
- Two-class deliverable structure (concerns C1):
  **CC-blocking** subtasks ship in M13's commits (CMake,
  scripts, docs, integration tests, soak runs).
  **Operator-blocking** subtasks (real-connection HW
  verification, clean-VM DEB install, GitHub Release publish)
  are recorded in M13-progress.md as pending and surfaced in
  M13-done.md hand-off.

## 1. Subtask sequence

| ID | Title | Net LOC est. | Output | Operator-blocking? |
|---|---|---:|---|---|
| S0 | M13-concerns.md (C1-C6) | ~250 | docs | no |
| S1 | CMake CPack config + install rules | ~250 | code | no |
| S2 | Post-install scripts + desktop entry + icon | ~80 | code | no |
| S3 | Documentation suite (release notes + install + spec list + combined HW protocol) | ~700 | docs | no (auth + verify) |
| S4 | 30-min memory soaks (M10 + M11) + results | ~50 | docs | partial (CC runs; operator may also re-run) |
| S5 | M13 integration tests + .deb structural validation | ~200 | code + tests | partial |
| S6 | M13-done.md + V1.0 freeze verification + sha256 collection | ~500 | docs | partial (operator confirms HW + DEB install before merge) |
| S7 | (Phase 3, next session) v1.0.0 tag + GitHub Release | n/a | release | yes (operator-authorized) |

Total CC-authored S0-S6: ~1 530 LOC. S7 is no LOC — just
sequenced authorized git + `gh release` operations.

## 2. Time budget

Spec target 3-5 person-days. CC wall-clock dominated by S4
(~60 min for soaks; backgrounded so concurrent doc-authoring
can continue) and S6 (sha256 harvest + done.md authoring).

## 3. HALT triggers (M13-specific, on top of CLAUDE.md §HALT)

| # | Trigger | Measurement point | Action |
|---|---|---|---|
| H1 | M2-M12 frozen `.hpp` modification | pre-commit `git diff` against M2-M12 freeze list (collected at S6) | HALT; revert; re-design |
| H2 | DEB build fails under CMake CPack | S1 close + S5 re-test | HALT; investigate CMake config |
| H3 | DEB install fails on clean Ubuntu (operator-reported) | S6 operator session | HALT; fix packaging |
| H4 | Hardware verification < 16/18 pass (operator-reported) | S5 / S6 operator session | HALT; investigate failures |
| H5 | 30-min soak fails (memory leak detected; > 10 % VmRSS growth) | S4 sweep | HALT; file ADR or hotfix |
| H6 | DEB launches but app crashes (operator-reported) | S6 operator session | HALT |
| H7 | DEB uninstall leaves files behind | S6 operator session | HALT; fix prerm |

Plus CLAUDE.md standard set (compile error 3×, test fail 3×,
new dep, frozen-interface mod, perf miss after 1 opt pass,
spec/arch contradiction, Qt 6.10 anomaly, two plausible
impls, unexplained git failure).

## 4. Subtask details

### S0 — Concerns

**Inputs**: spec §3, §7, §9 + this plan §3 + understanding §5.

**Deliverables**: `.claude/M13-concerns.md` documenting C1-C6
(per understanding.md §5) with resolution paths + subtask
anchors.

**Build / test**: docs-only.

**Done when**: concerns committed; default position holds
(no ADR-008 expected for M13).

### S1 — CMake CPack + install rules

**Deliverables**:
- `cmake/cpack-deb.cmake` per spec §4.1: CPack generator =
  DEB, package name `signalforge`, version `1.0.0`,
  dependency manifest, install dir `/opt/signalforge/`,
  `CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA` pointing to
  `cmake/deb-scripts/postinst` + `prerm`.
- Root `CMakeLists.txt`:
  - Version bump 0.0.1 → 1.0.0
  - `install()` rules for binaries (`signalforge`,
    `sfreplay_inspect`, `profile_main`)
  - `install()` rules for docs (release-notes, install,
    spec-list, m13-hardware-verification, M5-M12 baseline.md)
  - `install()` rules for tools (`tools/profile/run_profile.sh`,
    `check_regression.py`, `tests/benchmark/m12_regression_suite.sh`)
  - `install(PROGRAMS …/signalforge.desktop DESTINATION
    share/applications)` + icon
  - `include(cmake/cpack-deb.cmake)` at the end
- `installer/signalforge.desktop` (created in S2; referenced here)
- `installer/signalforge.png` (placeholder 256×256 created in S2)

**Build gate**: `cmake -B build/release -DCMAKE_BUILD_TYPE=Release` succeeds;
`cmake --build build/release --target package` produces
`build/release/signalforge_1.0.0_amd64.deb`;
`dpkg-deb --info <file>` shows expected control fields.

### S2 — Post-install / pre-removal scripts + desktop entry + icon

**Deliverables**:
- `cmake/deb-scripts/postinst`: bash script per spec §4.2.
  `chmod +x` set in CMake.
- `cmake/deb-scripts/prerm`: bash script per spec §4.3.
- `installer/signalforge.desktop`: per spec §4.4. Exec
  points at `/usr/local/bin/signalforge` symlink.
- `installer/signalforge.png`: 256×256 placeholder PNG
  (generated programmatically — solid color + simple text;
  V1.5+ may swap for designer art).

**Build gate**: scripts pass `bash -n` syntax check;
`.desktop` validates against `desktop-file-validate` if
available; final `.deb` includes all four files at the
expected paths.

### S3 — Documentation suite

**Deliverables**:
- `docs/release-notes/v1.0.0.md`: user-facing V1.0 release
  notes per spec §2.1-5. Sections:
  - V1 feature summary (live + record + replay; from
    M9/M10/M11 + M12 perf)
  - System requirements (Ubuntu 24.04, Qt 6.10)
  - Install instructions (link to install.md)
  - Quick start (5-step example)
  - Known limitations (V1.5+/V2 deferred items list)
  - Performance baseline summary (from M12-baseline.md)
  - Acknowledgements / MIT license
- `docs/install.md`: per spec §2.1-6. 3 install paths
  (.deb / source build / fallback). Permissions / udev for
  serial port. Troubleshooting.
- `docs/v1.0-spec-list.md`: V1.0 freeze record per
  spec §6.3 + §4.7. **All M2-M11 frozen `.hpp` sha256s**
  collected via `sha256sum`. 7 ADRs cross-referenced.
  Format specs (SFREPLAY v1, charts.yaml v1, connections
  yaml v1). M0-M13 spec list. V1 architectural invariants
  (single-threaded UI, lock-free reader path, bounded
  staleness ≤ 1 ms, real-time playback ±5 % per M12 S3).
- `docs/m13-hardware-verification.md`: combined 18-test
  protocol. Quotes M9 (6) + M10 (6) + M11 (6) protocols
  verbatim with section headers; adds V1.0 dogfood
  preamble + 16/18 acceptance bar from spec §5.2.

**Build gate**: docs-only commit; markdown-lint clean
(if available); cross-reference links resolve.

### S4 — 30-min memory soaks (M10 + M11)

**Deliverables**:
- `tests/benchmark/results/M13-final-soak.md` documenting:
  - M10 soak: `bench_session_writer --soak 1800
    --memory-snapshot 30` — VmRSS samples + final growth %
  - M11 soak: `bench_replay --memory-soak 1800
    --memory-snapshot 30` — same shape
  - Acceptance per spec §5.3: < 10 % VmRSS growth on each.

**Execution**: CC runs both soaks **sequentially** (not
parallel — avoids contention; cleaner data). Each is ~30 min
wall-clock. CC backgrounds the job and continues authoring
docs while it runs.

**HALT trigger evaluation**: H5 fires if either soak
reports > 10 % growth. Per spec §3.5 V acceptance, that is
a release blocker.

### S5 — M13 integration tests + .deb structural validation

**Deliverables**:
- `tests/integration/test_m13_deb_package.cpp`: builds the
  .deb (or assumes it's pre-built), runs `dpkg-deb --info`
  + `--contents`, asserts package metadata + file list
  match spec §2.1-3 expectations.
- `tests/integration/test_m13_release_artifacts.cpp`:
  asserts presence of all required files in the install
  tree (`/opt/signalforge/bin/*`, `docs/*`, etc.). Tests
  that `signalforge --version` (if implemented) returns
  `1.0.0`; `sfreplay_inspect --help` exits 0.
- Wired into `tests/integration/CMakeLists.txt`.

**Build gate**: ctest 587 +2 (M13 integration cases) =
**589/589** on Debug + Release.

### S6 — M13-done.md + V1.0 freeze verification

**Deliverables**:
- Run `sha256sum` on every M2-M11 frozen `.hpp` (the list
  is curated via M2-M11 done.md `§Freezes`). Record into
  `docs/v1.0-spec-list.md` + cross-check against done.md
  values. Mismatch = HALT (would indicate post-freeze drift).
- `.claude/M13-done.md`:
  - Spec §2.1 deliverables checklist (14 items)
  - PR + merge state placeholders (filled at PR creation)
  - **V1.0 freeze record** section (sha256 table for every
    M2-M11 frozen `.hpp` + 7 ADRs + format specs)
  - Acceptance self-check vs spec §8 (8.1-8.7)
  - **Release prerequisite status** table (4 gates):
    - M10 soak result (CC-collected at S4)
    - M11 soak result (CC-collected at S4)
    - 18-test HW verification status (operator-pending
      until executed)
    - DEB install verification status (operator-pending)
  - HALT trigger disposition
  - Hand-off section: V1.5+ / V2 deferred items, V1.5+
    optimization roadmap, "how to file V1.5+ proposal"
  - Phase 3 sequence (next session): merge → tag v1.0.0
    → push tag → publish GitHub Release

**Build gate**: docs-only.

### S7 — (Phase 3, next session) v1.0.0 tag + GitHub Release

**Not part of M13's S0-S6 commits.** Documented in plan for
visibility; executed in the next session after Phase 2
approval per CLAUDE.md §Git operation protocol Phase 3.

Per the plan-§3 sequence the next session will need:

```
authorized: gh pr merge <M13-PR> --merge --delete-branch=false
authorized: git tag -a v1.0.0 <MERGE-SHA> -m "SignalForge v1.0.0"
authorized: git push origin v1.0.0
authorized: gh release create v1.0.0 --title "SignalForge v1.0.0" \
    --notes-file docs/release-notes/v1.0.0.md \
    signalforge_1.0.0_amd64.deb
```

The `gh release create` step is **the V1.0 ship moment**.
Per CLAUDE.md §Forbidden #4, this requires per-operation
authorization from the next session's prompt.

## 5. Risk register + mitigation

| Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|
| Soak detects leak | Low | HALT (release blocker) | If detected, file ADR-008 + hotfix in V1.0.1; M13 documents finding and re-runs |
| CPack DEB build fails on host | Medium | HALT (S1 redo) | Spec §9 advice: read CMake CPack docs; iterate locally before claiming success |
| Qt 6.10 unavailable on stock Ubuntu 24.04 | Certain | install.md gap | Document the requirement in install.md (user installs Qt 6.10 from external repo); V1.5+ may bundle |
| 18-test HW verification has > 2 fails | Medium | HALT (release blocker) | Investigate per failure; docs/m13-hardware-verification.md authored to maximize first-pass success |
| Operator's clean Ubuntu VM not available | Low | M13 close blocked | M13-done.md surfaces operator-pending status; release prerequisite must be met before tag |
| sha256 mismatch on a frozen `.hpp` at S6 | Low | HALT (H1) | Investigate which commit modified it; should be impossible if CLAUDE.md discipline held |
| CMake `install()` paths wrong | Medium | DEB structure invalid | S5 integration test catches structural mismatches before close |

## 6. V1.5+ / V2 deferred items (mirror M13 §2.2)

V1.5+:
- Multi-platform support (macOS / Windows)
- Code signing / notarisation
- App store / repository submission (Ubuntu PPA, Flathub)
- Automated release CI
- Static linking of Qt
- Localization / i18n
- Extracted FHS-compliant docs path (`/usr/share/doc/signalforge/`)
- Per-user app data location options
- M11 backward seek O(N) → in-memory index (M11 §9 Note 2)
- M6 SignalBuffer push-wrapper Double-specific optimisation
  (M12 S4 H4 finding)
- SignalBufferRegistry per-event hashtable lookup (M12
  profile §6.6 candidate)
- Real live-mode profile (M12 deferred — synthetic Scenario A
  bypassed schema decoder)
- Theme / dark mode

V2:
- Network sync / live streaming
- Encryption / authentication on file format
- Plugin architecture
- arch.md §G two-file `.sfr` + `.sfi` index design (M10
  ADR-007 deferred)

## 7. Closeout checklist

- [ ] All S0-S6 commits landed on `milestone/M13`
- [ ] CI green on every commit
- [ ] PR opened to `main`; CI green on PR
- [ ] M13-done.md published with PR # / head SHA + freeze
      sha256s
- [ ] Phase 1 step 6 announce: "M13 ready. Awaiting approval
      to merge M13 and ship V1.0.0"

After Phase 2 approval, Phase 3 (next session) executes
S7: merge + v1.0.0 tag + GitHub Release.

---

## 8. What I am NOT planning to do

- Modify any M2-M12 frozen `.hpp` (default; ADR-008 + V1.0.1
  patch path required if forced).
- Add new V1 features.
- Add new top-level dependencies.
- Bundle Qt with the .deb (V1.5+ if needed).
- Run the operator-blocking gates myself (HW verification on
  real connections, DEB install on clean VM, GitHub Release
  publish under operator's account).
- Tag `v1.0.0` in M13 Phase 5 (Phase 3 work, next session).
- Skip the 30-min soaks (acceptance gates per spec §3.5 V).
- Claim DEB install verification "passed" without operator
  confirmation.

## 9. Phase 4 / 5 expectations

After this plan is approved (Phase 4 — "approved, execute
M13"), S0 begins immediately. Subtasks proceed sequentially
with CI watch between each. At S6 close, Phase 1 step 6 fires
and the M13 PR opens.

The discipline this milestone enforces — packaging-only,
operator-blocking gates surfaced honestly, V1.0 finality
respected — is the V1.0 release discipline. M13 is the last
chance to get V1.0 right; after `v1.0.0` is tagged, V1.0 is
locked.
