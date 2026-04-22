# M1 — Understanding

## 1. Restatement of the M1 goal

M1 is evidence-gathering, not implementation. I build a standalone
`tools/spike/qquick_dock_test/` program that puts three
`QQuickWidget` instances inside three `QDockWidget` containers of a
`QMainWindow`, then exercise five integration checks that mirror
what M6's chart will do (float/re-dock, HiDPI, context menus,
hide/show lifecycle, multi-instance GPU). The spike is isolated: it
is **not** added to the top-level `CMakeLists.txt`, it does not
touch `src/`, and it introduces no new top-level dependencies. The
final deliverable is `docs/spikes/M1-qtquick-integration.md` — a
verdict matrix plus per-check methodology, results, screenshots,
and fallback-impact notes — that lets the human decide, with
confidence and without my recommendation, whether `QQuickWidget`
can carry the V1 chart workload or whether the `QWindow`-container
or `QPainter`/OpenGL fallback from `[Arch §8.5]` needs to be
adopted before M6 begins.

## 2. Observed repo state (before any code)

Minor deviation from the session prompt, not blocking:

- `origin/main` is at `afe87aa docs: add M1 Qt Quick integration
  spike spec` (human-seeded M1 spec). Tag `v0.0.1-alpha.1` is at
  `40b89a2` — two commits behind current main (`e28b1a1` arch GCC
  13.3 fix + `afe87aa` M1 spec). Prompt said "Confirm main is at
  v0.0.1-alpha.1"; actual main is at v0.0.1-alpha.1 + 2 docs-only
  commits. Interpreting as benign — the tag still marks the M0
  baseline and milestone/M1 is a clean branch off current main
  (`milestone/M1 == origin/milestone/M1 == origin/main == afe87aa`,
  working tree clean).
- Default branch: `main`. Visibility: `PRIVATE`. Branch protection
  still deferred (OV-1 in `.claude/M0-overrides.md`).
- Side note, not acting on it: `afe87aa` appears to be a direct
  commit to `main` by the human. The OV-1 interim discipline says
  "human does not push to main directly either". Assuming the human
  intentionally chose to treat a one-file docs seed as exempt; I
  mention it purely so the record is complete.

## 3. Ambiguities and contradictions identified

For each, my default interpretation stated. If a default is wrong,
please correct at plan review, not mid-execution.

### 3.1 "Pass / Partial / Fail / Blocked" for the verdict matrix

The verdict matrix (§2.2) uses four states. §5 "soft-HALT" describes
**Partial** precisely (environmental failure recorded in
`.claude/M1-partial-results.md`). But §2.2 also introduces
**Blocked** without definition.

**Default interpretation**: "Blocked" means neither local nor CI
produced a usable signal (the check could not even attempt
evaluation; e.g., Check 5 locally has no GPU telemetry AND CI is
spec'd to skip Check 5 → the CI column is "N/A" but the local
column is "Blocked"). "Partial" means one side produced signal and
the other did not. "Fail" is a negative verdict from actual
execution. "Pass" is a clean positive. Documented in the report's
legend.

### 3.2 How many Partials count against the soft-HALT threshold

§5.4 says "M1 can complete successfully with up to two independent
subtasks in Partial state, provided S8 (CI headless subset) is
Pass". §6 HALT #3 says "Three or more independent subtasks hit
Partial state". These are consistent. But what if CI covers a
Partial local and turns it Pass globally? Does that Partial still
count against the threshold?

**Default interpretation**: the Partial count tracks **local**
verdicts. A check marked Partial (local) but Pass (CI) still counts
as Partial for the threshold — the soft-HALT exists to catch
degraded local environments, and CI compensation doesn't change
that diagnosis. The **Final verdict** column in the matrix,
however, can read "Pass (CI)" or "Partial (local) / Pass (CI)" so
the human sees both facts.

### 3.3 "No recommendation" vs. "fallback impact"

§2.3-6 forbids recommending go/no-go. §S3–S7 require "Fallback
impact if Check K fails" notes. These don't directly contradict —
impact-if-failure is factual, not recommendation — but the line
between "here's what a fail would mean" and "you should pick the
fallback" is thin.

**Default interpretation**: fallback-impact text describes
*consequences*, not *prescriptions*. Phrased as "If this check is
Fail, use case X cannot ship under QQuickWidget," not "If this
check is Fail, switch to the QWindow fallback." The report has
**zero** "I recommend", "we should", "the right choice is"
phrasings. The closing "Data for the human's decision" section
lists facts only.

### 3.4 Logging inside the spike

§S2 says `qDebug` is acceptable inside the spike as an exception to
`[CM §Forbidden-6]`, with the exception documented in the spike's
README. §9 reiterates this. But the CLAUDE.md rule lives in §Forbidden
which has no per-directory scope.

**Default interpretation**: the spec explicitly authorizes the
exception for `tools/spike/**`. I take the authorization at face
value. The spike's `README.md` (§2.4 item 1) gets a one-sentence
line: *"Spike uses `qDebug` for logging; production code under
`src/` uses `SF_LOG_*` per CLAUDE.md §Forbidden-6."*

### 3.5 Screenshot tool split (resolved at approval)

§S3–S5 reference `grim / scrot / import` "whichever is available".
Per approval update #2, the method splits by check:

- **Check 2 (HiDPI)**: use `scrot <filename>.png` for real-display
  capture. `QWidget::grab()` walks a software path and cannot
  evidence HiDPI rendering on a real compositor, so it would
  invalidate the Check 2 result.
- **Checks 1 / 3 / 4 / 5**: continue using `QWidget::grab()` —
  their assertions are behavioral (no crash, menu appears, no
  leaks, no GPU exhaustion), not pixel-fidelity, so a software
  path is sufficient and avoids X/Wayland tooling dependence.

### 3.6 `--auto-check` flag semantics

§S3–S7 introduce a `--auto-check K` command-line flag that runs
check K and exits. §S8's CI runs `--auto-check 1`, `--auto-check 3`,
and `--auto-check 4 --short`. A `--short` variant is mentioned only
for Check 4 and only in §S8.

**Default interpretation**: `--short` on `--auto-check 4` reduces
cycles from 20 to 5 (matching the "5-cycle" subset already
mentioned in §S6). Other checks do not have a `--short` variant —
default CI-friendly lengths are chosen per check (Check 1: 5
cycles, Check 3: 3 docks × one click each, Check 4 short: 5
cycles, Check 4 full: 20 cycles, Check 5: 30 s sampling).

## 4. Subtasks I expect to be environmentally failure-prone on this host

Pre-approval probe showed `valgrind`, `radeontop`, `scrot` all
missing. Approval update #1 states these are now installed; S1
verifies with `which` and HALTs if any are missing. Assuming that
verification passes, the local risk profile collapses substantially:

| Subtask | Local risk | Reason |
|---|---|---|
| S3 Check 1 (floating) | Low | `QDockWidget::setFloating` + `QTest::qWait` + `QWidget::grab()` screenshot. No external tools. X11 `DISPLAY=:0` available. |
| S4 Check 2 (HiDPI) | Low | X11 display present; `scrot` now installed (per update #2) for real-compositor capture at 4 scale factors. |
| S5 Check 3 (context menu) | Low | Event-propagation is framework-level; `QWidget::grab()` screenshot of menu popup. |
| S6 Check 4 (hide/show lifecycle) | Low–Medium | `valgrind` now installed; however `/etc/ld.so.preload` still loads `libAppProtection.so`, which broke local ASan in M0 (C2) by recursing with malloc interception. Valgrind's malloc instrumentation may collide the same way. Plan: run `valgrind --leak-check=full` once on `--short` (5 cycles); if it fails with an AppProtection recursion signature, record `check4-valgrind-blocked.txt` and fall back to the always-available `/proc/self/status` VmRSS + FD sampling arm — **one workaround attempt only**, then move on per M1 §5.3. Per update #5, this is an immediate-report event, not accumulated. |
| S7 Check 5 (multi-instance GPU) | Low | `radeontop` now installed for GPU%; VRAM sampled via `/sys/class/drm/card*/device/mem_info_vram_used` (per update #3), which does not depend on any telemetry tool. CPU% via `/proc/self/stat` deltas (no `top` dependency). |

**Checks I expect to run cleanly locally**: S3, S4, S5, S7, and the
non-valgrind arm of S6.

**Residual risk (one item)**: valgrind × AppProtection interaction
at S6. This is the only environmental unknown remaining. If it
fires, the S6 local verdict degrades to Partial (non-valgrind arm
only) and I report immediately.

**Checks I lean on CI for**:
- Per §S8: CI runs Checks 1, 3, and 4-short headless under
  xvfb. CI is corroborating evidence for locally-Pass checks and
  a fallback lifeline for any check that partials locally.
- Check 2 (HiDPI) is skipped in CI (no real display) — local is
  primary; human visual verification closes the crispness question.
- Check 5 (GPU) is skipped in CI (no GPU) — local is the only
  evidence source.

## 5. HALT risks

### 5.1 Hard HALTs (per CLAUDE.md §HALT) specifically applicable

- **Trigger 3** (new dependency): temptation to pull in
  `nlohmann/json` or similar to parse intel_gpu_top JSON. Not
  actually needed on this host (no intel_gpu_top anyway); if a
  different GPU tool's output needs parsing, `QByteArray`/
  `QJsonDocument` from Qt Core suffice. No new top-level deps.
- **Trigger 7** (spec vs architecture contradiction): unlikely.
  §8.5 and the M1 spec are consistent. No freeze conflicts possible
  since M0 established no freezes.
- **Trigger 9** (two plausible implementations): the programmatic
  vs. manual screenshot choice (§3.5 above) is the most likely
  place this fires. Recording my default interpretation
  pre-execution to keep this below the trigger.

### 5.2 M1-specific HALTs (§6 of the spec)

1. **Qt Quick cannot load at all** — `QQuickWidget::setSource`
   errors, or a blank render after 5 s on valid display. Moderate
   risk: Qt 6.10.2 + AMD integrated graphics + X11 is a combination
   that *should* work (RHI defaults to OpenGL on X11), but AMD +
   Mesa + QQuickWidget has historically been a flaky intersection.
   Would fire at S2 verification.
2. **Any blocking subtask fails** (S1, S2, S8, S9). Most realistic:
   S8 (CI workflow) failing for a real compile/link reason when the
   local build succeeds — Qt version drift between local (6.10.2 at
   `~/Qt/...`) and CI's `jurplel/install-qt-action@v4` install.
   Mitigated by using the same version pin (`'6.10.2'`) as the main
   CI workflow.
3. **Any independent Partial** (revised per approval update #5).
   With tools installed, the expected Partial count is 0. Any
   Partial during execution is reported immediately in chat rather
   than accumulated toward the soft-HALT threshold of 3. The only
   realistic trigger is S6's valgrind arm colliding with AppProtection.
4. **CI spike workflow fails for non-environmental reasons**. Same
   as (2) — would surface at S8 watch.

### 5.3 Repeat-M0 risks to pre-empt

- **AppProtection preload** (M0 C2) — already flagged above; the
  spike itself won't be built with ASan, so the first-order
  concern from M0 doesn't recur. Valgrind is where it may bite;
  mitigated by the non-valgrind arm of Check 4.
- **Branch protection absent** (OV-1) — M1 ends with the human
  merging milestone/M1 → main and tagging v0.0.2-alpha.1 (§8 of
  spec). I do not push to main directly, and the merge is the
  human's action, so the unprotected state doesn't matter for
  M1's execution.
- **Directly-pushed docs on main** — observed `afe87aa` appears to
  be a direct commit. Not acting on it; noted in §2 above.

## 6. What the plan will answer

The plan (next file) expands each S1–S9 into concrete output
files, local-vs-CI execution, and commit points. Following the
human-approval discipline in `[EM §3.3]` step 4, I stop after the
plan and wait for approval before touching any code.
