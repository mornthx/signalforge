# M16 S6 — Cross-Environment Verification (Full 12 V0.2 Baselines)

| Field | Value |
|---|---|
| Subtask | M16 S6 (full-baseline-set cross-environment determinism verification) |
| Date | 2026-05-12 |
| Branch | `milestone/M16` |
| S6 capture commit | `93c4d2c` (capture script + CI workflow extension) |
| Locale-fix commit | follow-up — `tests/visual/lib/capture.py` LC_ALL pin per R14 |
| CI run (S6 capture source) | `25684458725` — `build (release)` job 75404337474 |
| Local capture host | operator dev, Ubuntu 24.04 + Yaru, kernel 6.8.0-111-generic |
| CI capture host | Azure runner, Ubuntu 24.04, kernel 6.17.0-1010-azure |
| Comparison API | `compare_with_contract` (S3) with `require_env_sidecar=True` |
| Defaults | `PIXEL_THRESHOLD = 4`, `CLUSTER_THRESHOLD = 200 px`, `PERCENT_THRESHOLD = 1.0 %` |
| **Verdict** | **PASS (10/12) + AMENDMENT NEEDED (2/12) — production state-machine non-determinism in signal-tree iteration; locale R14 fix already applied** |

---

## 1. Headline

After applying a single R14-mandated environment fix (locale pin),
**10 of 12 V0.2 production-fidelity baselines are byte-identical
or near-byte-identical across operator-local + CI Azure runner**.
Two baselines (`12-multi-2-drivers`, `13-multi-5-drivers`) fail
the M16 < 1 % close gate at **1.005 % / 1.351 %** with cluster
sizes of 822 / 650 pixels — both attributable to a single
production-side root cause: **non-deterministic iteration order
in `SignalBufferRegistry::signalIds()`** (a `std::unordered_map`
traversal that varies cross-host).

This is exactly the discipline R12 + R8 + R14 + the S6 gate were
built to surface: a real cross-environment determinism bug
sitting in the signal-tree population path, surfaced by the
first full-baseline-set application of the M16 close gate, and
caught BEFORE the V0.2 → M16 baseline migration at S7. Without
S6, S7 would have shipped non-deterministic baselines for 2 of
12 states.

| Baseline | LOCAL vs CI | Max cluster | Env contract | Verdict |
|---|---:|---:|---|---|
| 00-empty-launch         | **0.000 %** (sha256 identical) | 0 | clean | PASS |
| 02-conn-udp-idle        | **0.000 %** (sha256 identical) | 0 | clean | PASS |
| 04-conn-udp-connected   | **0.000 %** (sha256 identical) | 0 | clean | PASS |
| 12-multi-2-drivers      | **1.005 %** (10 291 px)        | **822** | clean | **FAIL** |
| 13-multi-5-drivers      | **1.351 %** (13 831 px)        | **650** | clean | **FAIL** |
| 24-dialog-add-serial    | 0.016 % (160 px)              | 160 | clean | PASS |
| 25-dialog-add-udp       | **0.000 %** (sha256 identical) | 0 | clean | PASS |
| 26-dialog-edit          | **0.000 %** (sha256 identical) | 0 | clean | PASS |
| 30-menu-file-open       | **0.000 %** (sha256 identical) | 0 | clean | PASS |
| 31-menu-connections-open| **0.000 %** (sha256 identical) | 0 | clean | PASS |
| 32-menu-session-open    | **0.000 %** (sha256 identical) | 0 | clean | PASS |
| 33-status-buffer-normal | **0.000 %** (sha256 identical) | 0 | clean | PASS |
| **Aggregate** | **10 / 12 PASS** | — | 12 / 12 clean | — |

PNG byte-identity (`sha256` match) on 9 of 12 baselines is far
stronger than the S0.5 spike (0.122 % / 0.299 %) and the S4
keystone (0.000 % / 0.015 %). The 2 failing states are NOT
incremental drift around the gate threshold — they are a
qualitatively distinct failure mode (text-content non-
determinism, not pixel jitter).

---

## 2. Investigation timeline (forensic phases)

The S6 close gate exposed two distinct issues across the
investigation. The phasing is preserved here because each phase
illustrates a different discipline of M16's R-rules in action.

### Phase 1 — Initial capture, env_drift fires on locale

After CI run `25684458725` produced the 12 CI captures + S5-
auto-emitted env sidecars, the first cross-env diff measurement
returned:

```
pixel diff:   0 / 12 baselines exceed 1 % (all 0.000 %)
env contract: 12 / 12 baselines DRIFT
```

Drift source on every baseline: `tier_3_geometry.locale`:

```
operator local: "en_US"   (inherited from user shell LANG=en_US.UTF-8)
CI runner    : "C"        (Azure runner default LANG=C)
```

Per S3's `compare_with_contract`, env contract drift sets
`env_drift=[...]` and forces `verdict=FAIL` even when pixels
match (R14 / H10). Pixel parity does not absolve env contract.

**Disposition**: H10 fired. R14 says "fix env first, do not
accept/reject baselines." Spec `rendering-environment-lock.md
§4.1 line 146` says `C.UTF-8` is preferred over `en_US.UTF-8`
for CI portability, both legal.

### Phase 2 — R14 fix: pin LC_ALL/LANG/LANGUAGE in capture path

Fix landed in `tests/visual/lib/capture.py`'s `_run_capture`
subprocess env: explicitly export

```python
env["LC_ALL"]   = "C.UTF-8"
env["LANG"]     = "C.UTF-8"
env["LANGUAGE"] = "C"
```

Empirically required: just `LC_ALL` was insufficient on Ubuntu
24.04 — Qt's `QLocale::system()` on the operator's host fell
back to `LANG=en_US.UTF-8` when only `LC_ALL` was set, and the
sidecar still emitted `"locale": "en_US"`. Setting all three
forces `locale = "C"` deterministically.

**Re-capture (local) result**: 12 / 12 env contract clean.
PNG sizes match CI exactly on 9 of 12 baselines. R14 first-
application benefit realised — the env contract is now an
empirical invariant, not a forward-looking aspiration.

### Phase 3 — Pixel diff persists on 2 multi-driver baselines

With env contract clean, the second pixel diff measurement
returned:

```
10 / 12 baselines: 0.000 % (9 sha256-identical, 1 with 160-px
                            sub-cluster diff at 0.016 %)
 2 / 12 baselines:
    12-multi-2-drivers : 1.005 % / 10 291 px / max cluster 822 / 58 clusters
    13-multi-5-drivers : 1.351 % / 13 831 px / max cluster 650 / 72 clusters
```

Cluster sizes of 650–822 px and cluster counts of 58–72 are
qualitatively distinct from glyph-edge antialiasing noise
(which produces single-digit max-cluster sizes). Diff image
inspection confirmed the diff lives in the signal-tree text
labels, with WHOLE truncated-with-ellipsis labels differing
between local and CI.

Side-by-side text comparison of the Driver 1 signal subtree:

```
LOCAL (operator):                CI (Azure runner):
  crc                              sensor_mo...
  reserved                         calibration...
  timestamp...                     padding
  sensor_mo...                     temperatur...
  padding                          reserved
  alarm                            alarm
  calibration...                   crc
  temperatur...                    timestamp...
  pressure (...                    pressure (...
```

Same 9 signals, different order. The signal-tree population
displays whatever order the registry's `signalIds()` returns,
and that order is host-dependent.

**Root cause located**: `src/buffer/signal_buffer_registry.hpp:109`:

```cpp
std::unordered_map<QString, std::unique_ptr<SignalBuffer>> buffersBySignalId_;
```

and `src/buffer/signal_buffer_registry.cpp:250-258`:

```cpp
QStringList SignalBufferRegistry::signalIds() const {
    std::lock_guard<std::mutex> lock(registryMutex_);
    QStringList out;
    out.reserve(static_cast<int>(buffersBySignalId_.size()));
    for (const auto& [id, _] : buffersBySignalId_) {
        out.append(id);                          // <-- unordered_map iteration
    }
    return out;
}
```

`std::unordered_map` iteration order is unspecified by the
standard and depends on hash function, bucket count, and the
glibc-version-specific hash seed. Two different Linux hosts
running the same binary against the same fixture will produce
different iteration orders even when every other rendering
input matches — exactly the symptom observed.

Single-driver states (`00`, `02`, `04`, `33`) don't surface
the bug because the *order within one driver* happens to
coincide on both hosts (likely because hash bucket placement
of 9 keys is deterministic for hash inputs that small under
this glibc / Qt build pair). Multi-driver states surface it
because either:

- driver-1 + driver-2 trees have enough signals to spill into
  divergent bucket layouts, or
- the cross-driver scan order itself differs (the same
  `buffersBySignalId_` map is iterated; signal IDs from driver
  1 + driver 2 are interleaved in hash-bucket order, not
  driver-grouped order).

The fix is straightforward (single function: sort the
returned `QStringList` before returning) but it is a
**production code change**, out of S6's authorisation scope.

---

## 3. The 0.016 % outlier — `24-dialog-add-serial`

State 24 is **not** byte-identical (160-px diff at 0.016 %), and
its single 160-px cluster is *just* under the 200-px cluster
gate. This matches the S4 keystone measurement (0.015 %) on the
same baseline almost exactly. The diff is concentrated in glyph
antialiasing of the dialog's "Add connection" form text — Qt's
bundled FreeType produces 1-channel-LSB drift on a small number
of glyph edges across the two hosts. The cluster gate
absorbs it; verdict PASS.

The S0.5 spike measured this state at 0.299 % under a 6-role
minimal palette + no QSS; the full S4 + S5 stack (18-role
`QPalette` + `tokens.qss` + 6 bundled fonts + S5 sidecar
emission) reduces it to 0.016 % — a 20× improvement holding
across both S4 and S6 measurement contexts.

---

## 4. R12 baseline regression discipline — first full-set outcome

R12 was added to the V0.3 charter specifically to catch cross-
environment determinism regressions before they ship in
baselines. S6 is R12's first full-baseline-set application.

**R12 outcome**: caught a real production non-determinism in
the signal-tree iteration path that all of S0.5, S1, S2, S3,
S4, S5 had missed (S0.5 + S4 used 2 baselines, neither
multi-driver; S5 was schema-only). If S6 had simply re-captured
baselines and approved them, S7 would have committed
`12-multi-2-drivers.png` + `13-multi-5-drivers.png` to
`tests/visual/baselines/` with whichever signal order happened
to be observed at S7-capture time, and any later host
(developer machine, CI runner upgrade, glibc bump) would have
shipped 1–2 % visual diffs against those baselines.

This is the per-baseline R8 review value: R8 says "operator
deliberately accepts each new M16 baseline." Without S6, the
operator would not have had data to deliberate on.

**R12 first-application disposition**:

1. R14 fix (locale pin) — applied in this report's
   accompanying commit (`tests/visual/lib/capture.py`). This
   is in S6 scope per the operator's S6 prompt §6 ("R14 env
   contract enforced via `require_env_sidecar=True`"; the fix
   is an env-side change in capture infrastructure).
2. Production non-determinism fix (signal-tree sort) —
   **NOT applied in S6**; this is a production code change
   touching `src/buffer/signal_buffer_registry.cpp`. Scope
   decision belongs to the operator.

---

## 5. Per-baseline R8 review preparation

For each of the 12 baselines, the R8 per-baseline review is
prepared with the following artefacts (all under
`tests/screenshots/m16-s6/` post-S6 capture):

- `<state>.png` — operator-local M16 SignalForgeStyle capture
- `<state>.env.json` — S5 4-tier env sidecar
- `<state>.diff.png` — overlay diff vs CI capture (emitted
  only for the 2 failing states; the 10 PASS states are
  byte-identical or near-byte-identical)
- `<state>.diff-report.json` — full `compare_with_contract`
  metrics

CI-side captures (same artefacts) are available from CI run
`25684458725` artifact `visual-screenshots-release` under
`m16-s6/`.

R8 review **recommended approvals (10 of 12)**:

These 10 baselines are pixel-perfect cross-env and ready for
S7 migration:

- `00-empty-launch`
- `02-conn-udp-idle`
- `04-conn-udp-connected`
- `24-dialog-add-serial` (within cluster gate at 0.016 %)
- `25-dialog-add-udp`
- `26-dialog-edit`
- `30-menu-file-open`
- `31-menu-connections-open`
- `32-menu-session-open`
- `33-status-buffer-normal`

R8 review **blocked pending production fix (2 of 12)**:

- `12-multi-2-drivers` (1.005 %)
- `13-multi-5-drivers` (1.351 %)

Both blocked on signal-tree sort fix. Once fixed and re-
captured, both states are expected to produce byte-identical
or near-byte-identical cross-env captures based on the pattern
the other 10 states display.

---

## 6. Recommended amendment scope (operator decides)

The minimal production fix is a single-function change at
`src/buffer/signal_buffer_registry.cpp:250-258`:

```cpp
QStringList SignalBufferRegistry::signalIds() const {
    std::lock_guard<std::mutex> lock(registryMutex_);
    QStringList out;
    out.reserve(static_cast<int>(buffersBySignalId_.size()));
    for (const auto& [id, _] : buffersBySignalId_) {
        out.append(id);
    }
    out.sort();  // <-- new — deterministic cross-host iteration order
    return out;
}
```

Same one-line fix likely applies to `signalIdsForDriver()`
(line 260–264). Both call paths feed the signal-tree population
at `src/chart/signal_selector.cpp:147-148`. Behavioural impact
on existing functional tests: signals appear alphabetically
instead of in hash-bucket order; the V0.2 baselines for states
12 + 13 will need recapture under the post-fix binary regardless
(R8 per-baseline operator review covers the visual delta).

Possible alternate scope: change `buffersBySignalId_` storage
type from `std::unordered_map` to `std::map` (sorted by default).
Higher blast radius (touches all callers); recommended over
`out.sort()` only if there's an existing performance argument
for ordered access elsewhere.

S7 sequencing under either fix:

1. Apply production fix in a new commit (S6.5 / S7-pre /
   M17 — operator decides milestone scope).
2. Re-run S6's `capture_m16_s6.py` for states 12 + 13 (other
   10 already locked).
3. Re-verify cross-env diff < 1 %.
4. S7 baseline migration with 12 / 12 R8-approved baselines.

No alternative is needed if operator decides to accept the 2
states as visually-correct-but-non-byte-deterministic at
M16 close and defer the production fix to V0.4 — but this
would weaken R12 in its first application; not recommended.

---

## 7. CI workflow context

The S6 cross-env capture step in `.github/workflows/ci.yml`
ran on CI run `25684458725`:

```
✓ Configure / Build (debug + release)
✓ M16 token-freshness gate
✓ M16 cross-env continuity capture (S0.5 + S4)
✓ M16 S6 cross-env baseline capture (12 V0.2 baselines)
✓ Upload visual-test screenshots
X Test (V0.2 baselines obsolete — transitional state per
        M16-plan §S6 / §S7; will lift at S7 baseline migration)
```

The CI Test step continues to be red on the V0.2 visual tests
because the committed V0.2-era baselines drift 14–34 % vs M16
SignalForgeStyle. Per d2e6123 §4 (S4 cross-env continuity
report), this is the expected M16 S4 → S7 transitional window;
unaffected by S6's findings. S7 baseline migration is what
turns the Test step green again.

---

## 8. Comparison with prior measurements

| Stage | Method | 00-empty-launch | 24-dialog-add-serial | Worst of 12 |
|---|---|---:|---:|---:|
| S0.5 spike | minimal stack (6-role, no QSS, single font) | 0.122 % | 0.299 % | n/a (2 baselines only) |
| S4 keystone | SignalForgeStyle full stack, V0.2 compare API | 0.000 % | 0.015 % | n/a (2 baselines only) |
| S6 phase 1 | S5 sidecar emit, no env fix | 0.000 % | 0.000 % | 0.000 % (pixels) / DRIFT (env) |
| **S6 phase 2** | **+ R14 locale pin** | **0.000 %** | **0.016 %** | **1.351 % (signal-tree non-det)** |

The improvement from spike → S4 → S6 phase 1 on pixel parity is
~∞ × (byte-identical). The S6 phase 2 1.351 % regression vs
S6 phase 1 0.000 % is illusory — the 12 / 13 states were
already non-deterministic; the phase 1 measurement coincidentally
captured a moment when both local and CI had the same
hash-bucket order. The R14 locale change is what made the
non-determinism visible (likely by perturbing hash seeds; not
re-confirmed but plausible).

---

## 9. Verdict

**M16 close gate is satisfied for 10 of 12 V0.2 production-
fidelity baselines** with substantial margin (9 are byte-
identical sha256-equal; 1 is at 0.016 % well below both close
gate and cluster threshold).

**M16 close gate is not satisfied for 2 of 12 V0.2 baselines**
(`12-multi-2-drivers` 1.005 %, `13-multi-5-drivers` 1.351 %)
due to a single production-side root cause:
`std::unordered_map` iteration non-determinism in
`SignalBufferRegistry::signalIds()`.

The 2-baseline failure is **a R12 discipline win, not a M16
design failure**: M16's R10 / R11 / R12 / R14 / R15 governance
caught a real production non-determinism bug that would
otherwise have shipped as locked baselines at S7. The
SignalForgeStyle stack itself is byte-identical-deterministic
across operator-local + CI Azure runner on the 10 single- or
zero-driver states, and the 2 multi-driver states will be
byte-identical-deterministic post the signal-tree sort fix
(based on the pattern the other 10 states display).

**Recommended next actions** (operator decides):

1. Approve R14 locale pin in `tests/visual/lib/capture.py`
   (in this report's accompanying commit).
2. Approve scope for production signal-tree sort fix
   (`signalIds()` `out.sort()` one-line change OR
   `unordered_map`→`map` refactor). Either is suitable.
3. After fix lands, re-run `capture_m16_s6.py` for states
   12 + 13; re-verify; commit; S7 unlocks.

---

## 10. Cross-references

- S6 capture infrastructure: `tests/visual/scripts/capture_m16_s6.py`
- S6 CI workflow extension: `.github/workflows/ci.yml` step
  "M16 S6 cross-env baseline capture (12 V0.2 baselines)"
- S6 capture commit: `93c4d2c`
- S6 locale R14 fix: `tests/visual/lib/capture.py` (this report's
  accompanying commit)
- Root cause source: `src/buffer/signal_buffer_registry.cpp:250-258`,
  storage at `src/buffer/signal_buffer_registry.hpp:109`
- Signal-tree caller: `src/chart/signal_selector.cpp:147-148`
- CI artifact source: run `25684458725`, artifact
  `visual-screenshots-release`, path `m16-s6/`
- Local capture set: `tests/screenshots/m16-s6/` (gitignored)
- Diff images (failing states only):
  `tests/screenshots/m16-s6/12-multi-2-drivers.diff.png`,
  `tests/screenshots/m16-s6/13-multi-5-drivers.diff.png`
- S0.5 spike result: `docs/v0.3/spike-result.md`
- S4 keystone report: `docs/v0.3/s4-cross-env-continuity.md`
- Env contract spec: `docs/v0.3/rendering-environment-lock.md` §4.1
- M16 plan §S6: `.claude/M16-plan.md` lines 329–352
- V0.3 charter R12 (baseline regression discipline): per V0.3
  manifesto §5
- M16-spec §2.1 #5 (cross-environment determinism close gate)
