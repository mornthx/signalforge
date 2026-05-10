# M14 — Concerns (C1-C6)

Resolves the open questions raised in
`.claude/M14-understanding.md` §5. Resolutions confirmed by
the human at the Phase 4 / Phase 5 hand-off (the
"NOTABLE OBSERVATIONS" + "S0 CONCERNS PRIORITIES" sections of
the execute-M14 authorization message).

Each concern: question, options considered, resolution +
rationale.

---

## C1 — CI smoke-test approach (Approach 1 vs Approach 2)

**Question**: spec §4.1 sketches two approaches:
- **Approach 1**: C++ Catch2 test driving a `QProcess`-spawned
  release binary, using `QImage`/`QProcess` for Tier A pixel
  capture and `QProcess::readAllStandardError()` for Tier B
  log grep.
- **Approach 2**: shell-script harness using offscreen Qt
  launch, Python+PIL (or ImageMagick) for pixel diff, `grep`
  for log patterns.

CLAUDE.md §1 forbids new top-level dependencies; both
approaches must use already-installed tooling.

**Options considered**:
- A1 (C++/QProcess): keeps the test inside Catch2 +
  ctest discovery, no Python/PIL dep, matches the M13
  `tests/integration/test_v1_live_mode_pipeline.cpp`
  pattern. Higher initial complexity (QProcess lifecycle,
  IPC for chart capture) but better ctest integration.
- A2 (shell+Python): smaller per-line code surface; trivial
  to debug end-to-end with `bash -x`; but adds Python+PIL as
  a new top-level dependency unless we use only stdlib +
  ImageMagick (already a system package).

**Resolution**: **Approach 2 (shell+Python)** per the
human's recommendation in the Phase 5 message. Constraints:
- Use only Python 3 stdlib (no PIL / Pillow). Pixel-diff
  helper reads PNG via `struct`-based PNG decode, OR uses
  ImageMagick `compare` / `identify` (already in the Ubuntu
  24.04 base for `desktop-file-utils` and friends; verify
  at S1).
- Shell script lives at `tests/ci/release_binary_smoke.sh`
  but is **invoked by a Catch2 test** at
  `tests/integration/gui/test_release_binary_smoke.cpp`
  (one-line `QProcess::execute` of the shell script) so the
  test still appears in ctest. Best of both: shell-script
  debuggability + ctest discovery.
- Tier A capture: app exposes `--dump-chart-png <path>`
  (or `SIGUSR1` handler) for the smoke harness to extract
  the chart canvas; CC adds the hook in `main_window.cpp`
  during S1.

Trade-off accepted: dual-language test (shell + C++). The
shell harness is the primary; the C++ wrapper is plumbing.

---

## C2 — Operator-pairing cadence (S3 audit)

**Question**: S3 GUI audit needs interleaved CC↔operator work.
What cadence?

**Options considered**:
- C2a — Daily ping-pong: operator runs a subset of paths,
  posts findings; CC builds smoke-test extensions + fixes;
  operator re-tests. ~1-2 round-trips per audit day.
- C2b — Batch review: operator runs the full audit matrix in
  one session; CC fixes everything in a batch; operator
  re-runs the full matrix.
- C2c — Path-by-path serial: operator runs one path; CC fixes
  if broken; operator re-tests; move on. Fine grain, slow.

**Resolution**: **C2a — daily ping-pong** per the human's
Phase 5 recommendation. Concrete protocol:
- Operator runs **one section** of the spec §3.2 matrix per
  day (e.g., "live-mode chain" Monday, "recording chain"
  Tuesday, etc.).
- Operator posts findings as a numbered list in
  `.claude/M14-progress.md` (CC will scaffold the format
  during S0).
- CC builds smoke-test extensions for the section as soon as
  findings come in; fixes the Critical bugs surfaced; pushes;
  CI verifies.
- Operator re-runs the same section the next day to confirm
  fix; if green, move to the next section.

Trade-off: total wall-clock 7-10 days for the matrix,
matching plan §2's "calendar likely" estimate.

---

## C3 — PR strategy for `milestone/M14` closure

**Question**: PR #24 (M13) is OPEN. M14 work lands on
`milestone/M14`. Three closure paths in plan §6:
- (a) Open fresh PR `milestone/M14 → main` superseding #24
- (b) Sub-PR `milestone/M14 → milestone/M13`; #24 carries
  combined work
- (c) Cherry-pick M14 commits onto `milestone/M13`; close
  `milestone/M14`

**Resolution**: **Option (a) — `milestone/M14` supersedes
PR #24** per the human's Phase 5 message:

> At S0: close PR #24 with comment "Superseded by
> milestone/M14 (V1.0 GUI integration audit). M13 work
> absorbed into M14 closure PR."
> At S7: create fresh PR milestone/M14 → main as the V1.0
> release PR

**Action at S0** (this commit's sibling):
1. `gh pr close 24 --comment "Superseded by milestone/M14
   (V1.0 GUI integration audit). M13 release-prereq work
   (S7+S8+S8.1+S8.2 ADRs 008/009/010) is included in
   milestone/M14 history; M14 closure PR will be the V1.0
   release PR."`
2. PR #24 stays in CLOSED-without-merge state. Its history
   is preserved as a closure record.
3. `milestone/M13` branch stays alive on origin (no delete),
   solely as a freeze record.

**Action at S7**:
1. `gh pr create --base main --head milestone/M14 --title
   "M14: V1.0 GUI Integration Audit (Scenario X — V1.0
   <outcome>)"` with body referencing the closed PR #24 for
   traceability.

Trade-off: cleanest history; clear single PR for V1.0
review; no merge ambiguity.

---

## C4 — CI smoke-test location

**Question**: spec §2.1 #1 names two paths:
`tests/integration/gui/` or `tests/ci/`.

**Options considered**:
- `tests/integration/gui/`: ctest-discovered (existing
  pattern in M13's `tests/integration/`); fits M14 plan §6
  framework asset; requires Catch2 wiring.
- `tests/ci/`: standalone CI-only path; bypasses ctest
  altogether; simpler shell invocation.

**Resolution**: **`tests/integration/gui/`** per the human's
Phase 5 recommendation, matching the M13 pattern.

**Layout**:
```
tests/integration/gui/
├── CMakeLists.txt
├── test_release_binary_smoke.cpp   ← Catch2 wrapper
├── release_binary_smoke.sh         ← shell harness (C1)
├── chart_pixel_check.py            ← Tier A pixel diff
└── helpers/
    ├── udp_fixture_sender.py       ← UDP frame injection
    ├── log_grep.sh                 ← Tier B patterns
    └── fixtures/
        └── m14_smoke.yaml          ← test connection config
```

`tests/ci/` not used in M14. `--auto-load-test-fixture` is
an absolute path to the fixture yaml; the shell harness
copies the fixture to a temp dir before launching.

---

## C5 — Scenario decision pre-commitment guard

**Question**: M14.5 X authorizes three V1.0 outcomes
(A full / B reduced / C cancelled). The plan must explicitly
prevent CC from pre-committing to Scenario A out of
optimistic bias.

**Resolution**: **mandatory reminder in
`.claude/M14-progress.md`** (scaffolded by S0 alongside this
concerns doc):

```
## Scenario decision discipline (C5 reminder)

Per spec M14.5 X, V1.0 ships in one of three forms after
audit. CC must NOT pre-commit to Scenario A.

The decision is made post-audit, written collaboratively
with the human, finalized in `docs/v1.0-scope-evaluation.md`
at S5.

Triggers that should bias toward Scenario B / C:
- Architectural fix requires modifying > 2 frozen .hpp files
  (HALT H5)
- > 10 Critical bugs in S3 audit (HALT H3)
- 18-test HW verification < 12/18 (HALT H4)
- Audit reveals fundamental unfixable issues (HALT H1)

Until S5: do NOT phrase commits / progress updates / done.md
drafts as if Scenario A is the outcome. Use neutral language
("the V1.0 scope decision in S5 will determine ...").
```

CC will reference this section before authoring each S4 / S5
commit message and the S7 done.md.

---

## C6 — Frozen `.hpp` modification counter

**Question**: spec HALT #5 fires at `> 2` frozen `.hpp`
modifications. Where is the running count kept?

**Resolution**: **`.claude/M14-progress.md` §"Frozen-surface
modifications"** (scaffolded by S0). Initial state: 0/2.

The list of frozen `.hpp` files comes from
`docs/v1.0-spec-list.md` §1 (26 files; sha256s captured at
M13 S6). CC compares pre-fix sha256 against post-fix sha256
for any `.hpp` in the frozen list before each S4 commit. If
the count would exceed 2, HALT #5 fires before the commit.

Note: ADR-008 (M5 `decoder_registrar.hpp` additive method)
is the one M2-M13 frozen-surface modification on the
ancestor history. M14 starts the counter at 0/2 because the
ADR-008 modification is already shipped on
`milestone/M13` and M14 inherits that as its baseline.

If S4 finds an architectural fix that needs > 2 mods,
escalate per HALT #5 to V1.0 scope re-evaluation at S5.

---

## Summary

| Concern | Resolution |
|---|---|
| **C1** smoke-test approach | **Approach 2** (shell+Python stdlib only); Catch2 wrapper invokes the shell harness |
| **C2** operator pairing | **Daily ping-pong** by spec §3.2 section |
| **C3** PR strategy | **Option (a)**: close PR #24 at S0 with supersede comment; open fresh `milestone/M14 → main` PR at S7 |
| **C4** smoke-test location | **`tests/integration/gui/`** (ctest-discovered) |
| **C5** Scenario A pre-commit guard | Mandatory reminder section in `M14-progress.md`; neutral commit language until S5 |
| **C6** frozen-surface counter | Tracked in `M14-progress.md`; baseline 0/2 (ADR-008 inherited from M13); HALT #5 at > 2 |

All six are documentation-only. No code-touching work in S0.
S0 commit also closes PR #24 per C3.
