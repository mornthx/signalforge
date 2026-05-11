# Visual-Diff Algorithm Contract

| Field | Value |
|---|---|
| Status | Draft (S1 deliverable; locks at S1 Phase 4 operator review) |
| Companion to | `docs/v0.3/visual-identity.md` (manifesto) + `docs/v0.3/rendering-environment-lock.md` (env contract) |
| Implementation target | M16 S3 (`tests/visual/lib/compare.py` extension; QSS selector linter) |
| Empirical foundation | `docs/v0.3/spike-result.md` (0.12 % / 0.30 % cross-env diff at S0.5) |
| Authority | M16 spec §2.1 #2; §4.1 (algorithm sketch); §6 H10 (R14 env-drift INVALID) |

Defines exactly what "visual-diff < 1 %" means for the M16
close gate and all subsequent V0.3 regression detection.
Without this contract, "deterministic" is unfalsifiable.

---

## 1. Algorithm

For two PNG images `A` (baseline) and `B` (capture):

### Step 1 — Pre-check: environment contract compliance

Per `docs/v0.3/rendering-environment-lock.md` (R14 / H10):

- Both images must have an env.json sidecar at
  `<image_path>.env.json` OR `<image_path_stem>.env.json`.
- Sidecars are read and compared against the **required tier**
  of the env contract.
- If any required field differs between A.env.json and
  B.env.json: diff is **INVALID** — not soft-fail.
  - Return `CompareResult(matched=False, note="env-contract-violation: <field>", invalid=True)`
  - Test harness treats `invalid=True` as a HALT (H10), not a
    regression. The cause is environment drift, not code change.

If the env contract is honored, proceed to Step 2.

### Step 2 — Pre-check: image size match

- `A.width == B.width AND A.height == B.height`
- Size mismatch: **FAIL immediately**, no further computation.
  - Return `CompareResult(matched=False,
    note="dimension-mismatch: actual=<W>x<H> baseline=<W>x<H>")`

Industrial signal-tooling precedent: Saleae / Tektronix /
LabVIEW screenshots have stable dimensions per chosen capture
geometry. Different dimensions = different setup, not visual
regression.

### Step 3 — Optional masking pass

- Read `<state>.mask.json` from the baseline's directory
  (sibling file). Absent = no masking.
- Mask file schema (JSON):

  ```json
  {
    "regions": [
      {"x": 100, "y": 200, "w": 200, "h": 50,
       "rationale": "FPS readout — dynamic"},
      {"x": 50, "y": 700, "w": 300, "h": 30,
       "rationale": "Replay position label — known dynamic"}
    ]
  }
  ```

- Masked pixels are excluded from steps 4 + 5.
- Mask regions must have a non-empty `rationale` for human
  audit; the visual-diff tool emits a warning if rationale is
  empty (operators see drift in masked regions can't be
  audited).

Mask regions exist to handle inherently dynamic content (live
FPS counters, real-time clocks, frame numbers). Per R8: every
mask region requires per-state operator approval before commit.

### Step 4 — Primary metric: per-pixel channel-delta

For each non-masked pixel position (x, y):

```
A_rgb = A.pixel(x, y)    # 8-bit sRGB triple
B_rgb = B.pixel(x, y)    # 8-bit sRGB triple

delta_r = abs(A_r - B_r)
delta_g = abs(A_g - B_g)
delta_b = abs(A_b - B_b)
delta   = max(delta_r, delta_g, delta_b)

pixel_differs = (delta > PIXEL_THRESHOLD)
```

Result: `percent_differing = (count(pixel_differs) / total_non_masked_pixels) * 100`.

**Alpha channel handling**: SF baselines are captured opaque
(devicePixelRatio=1.0; xvfb framebuffer). Alpha is 255 for all
captured pixels. The contract assumes opaque; alpha is **not
compared**. If a future capture introduces transparent pixels
(e.g. custom widget with semi-transparent overlay), the
contract extends to compare alpha as a 4th channel; at M16 the
algorithm reads RGBA but only diffs RGB.

**Color space**: sRGB 8-bit per channel, as decoded by
`tests/visual/lib/compare.py`'s stdlib PNG reader. Linear-light
conversion is not applied (would be a contract change requiring
operator approval).

### Step 5 — Secondary metric: clustering check

Build the set of differing pixel positions
(`pixel_differs = True`). Compute 4-connected contiguous
regions via standard connected-component labelling (BFS / DFS).

For each region: count pixels in the region. Compute
`max_cluster_size = max(region_size for each region)`.

If `max_cluster_size > CLUSTER_THRESHOLD`: flag for review.

Rationale: a uniform scatter of differing pixels (font
antialiasing micro-drift, JPEG-like quantization noise, sRGB
gamma rounding) does not cluster — individual antialiased
edge pixels are spatially isolated. A genuine UI regression
(misaligned widget, relocated label, new control) produces a
clustered region. The clustering metric catches "small percent
but visually obvious" regressions that the percent metric
alone misses.

### Step 6 — Acceptance

```
PASS if:
    percent_differing < PERCENT_THRESHOLD
  AND
    max_cluster_size <= CLUSTER_THRESHOLD

FAIL otherwise
```

Return `CompareResult(matched=PASS, percent_differing=...,
max_cluster_size=..., note="diff=<...>%/cluster=<...>")`.

---

## 2. Tunable parameters

### 2.1 Defaults

```
PIXEL_THRESHOLD   = 4       # per-channel absolute delta (out of 255)
CLUSTER_THRESHOLD = 200     # max contiguous diff cluster (pixels)
PERCENT_THRESHOLD = 1.0     # max % of pixels differing
```

### 2.2 Empirical justification (per S0.5 measurement)

S0.5 R13 spike measured cross-env diff under the M16 stack:

| Baseline | Diff |
|---|---:|
| 00-empty-launch | 0.122 % |
| 24-dialog-add-serial | 0.299 % |

Both are **two orders of magnitude under** PERCENT_THRESHOLD =
1.0 %. The 1.0 % gate is not aspirational — it is
**empirically achievable with substantial margin** under the
M16 stack. PIXEL_THRESHOLD = 4 absorbs:

- sRGB 8-bit quantisation noise (delta of 1–2 per channel).
- FreeType antialiasing micro-variation (delta of 1–3 per
  edge pixel).
- Sub-pixel positioning rounding (within `addApplicationFont`-
  loaded font metrics).

PIXEL_THRESHOLD = 4 does NOT absorb:

- Font-family change (different glyphs entirely).
- Color-token change (intentional palette shift).
- Widget-position change (layout-engine differences).
- Spacing-token change (margin / padding shifts).

CLUSTER_THRESHOLD = 200 px catches:

- A relocated 10-character label (≈ 10 × 8 = 80 px per
  character × 3 chars worth of mismatch ≈ 240 px clustered).
- A misaligned button (1 button face area ≈ 80 × 24 = 1920 px,
  not all differing but large enough cluster forms).
- A new chrome region (e.g. an unintended status indicator
  appears) ≈ 100+ contiguous px.

CLUSTER_THRESHOLD = 200 does NOT catch:

- Font antialiasing micro-drift (scattered per-glyph-edge
  pixels, no large cluster).
- 1-pixel border drift on a panel separator (1-pixel-wide
  cluster only).

### 2.3 Per-state override mechanism

States with known acceptable variance (e.g. state 13
multi-5-drivers FLAKY at V0.2 close due to signal-selector
layout reflow) may have a sidecar override at
`tests/visual/baselines/<state>.thresholds.json`:

```json
{
  "pixel_threshold": 4,
  "cluster_threshold": 200,
  "percent_threshold": 1.5,
  "rationale": "Signal-selector reflow at 5-driver count;
                deferred to M17 widget rebuild for fix.",
  "approved_by": "operator@2026-06-MM",
  "review_at": "M17 close"
}
```

Each per-state override requires:
- `rationale` field — why the wider threshold is acceptable.
- `approved_by` field — operator who approved (R8 per-state
  authority).
- `review_at` field — when to revisit (which milestone closes
  the underlying issue).

Per-state overrides are **discouraged**. Default thresholds
work for 11/12 V0.2 production-fidelity baselines (per S0.5
measurement; only state 13 had FLAKY variance under V0.2).
M16 baselines re-captured under deterministic rendering should
not need any per-state override.

### 2.4 Theme variants

When M20 ships dark theme, the same algorithm applies to
dark-theme baselines (with their own `.env.json` declaring
`theme: dark`). PIXEL_THRESHOLD may need adjustment if dark-
theme antialiasing produces different micro-noise; that's an
M20 decision, not M16.

---

## 3. Diagnostic report

When `compare_with_contract()` returns `matched=False`, the
diagnostic includes:

```python
@dataclass
class CompareResult:
    matched: bool
    invalid: bool = False    # True only on env-contract violation
    width: int
    height: int
    percent_differing: float
    differing_pixels: int
    total_pixels: int
    masked_pixels: int       # pixels excluded by mask
    max_cluster_size: int
    cluster_count: int
    note: str
```

Plus a structured failure report saved to
`tests/screenshots/<state>.diff-report.json`:

```json
{
  "state": "00-empty-launch",
  "baseline": "tests/visual/baselines/00-empty-launch.png",
  "actual": "tests/screenshots/00-empty-launch.png",
  "verdict": "FAIL",
  "metrics": {
    "percent_differing": 1.234,
    "max_cluster_size": 450,
    "cluster_count": 12,
    "masked_pixels": 0
  },
  "thresholds": {
    "pixel_threshold": 4,
    "cluster_threshold": 200,
    "percent_threshold": 1.0
  },
  "env_drift": [],
  "top_clusters": [
    {"size": 450, "bbox": {"x": 100, "y": 200, "w": 30, "h": 18},
     "centroid": {"x": 115, "y": 210}}
  ],
  "diff_image_path": "tests/screenshots/00-empty-launch.diff.png"
}
```

Plus optional `<state>.diff.png` (visualisation of which pixels
differ; red overlay on grey baseline). Produced when
`compare_with_contract(..., emit_diff_image=True)`; useful for
operator review of borderline failures.

---

## 4. Backward compatibility with V0.2 API

The V0.2-era `tests/visual/lib/compare.py` exports
`compare_baseline(actual, baseline, max_diff_percent,
channel_tolerance)`. M16 S3 preserves this signature for the
4 existing test files (`test_states_empty.py`,
`test_states_with_connection.py`,
`test_states_chart_visible.py`,
`test_states_production_fidelity.py`) until S7 baseline
migration completes.

The new canonical API is
`compare_with_contract(actual, baseline, mask=None,
pixel_threshold=4, cluster_threshold=200,
percent_threshold=1.0, emit_diff_image=False)`.

After S7, all visual tests migrate to `compare_with_contract`;
`compare_baseline` is deprecated but not removed in M16 (M17+
removal candidate).

---

## 5. CI gate integration

`.github/workflows/ci.yml` (extended at M16 S5) runs ctest as
before. The visual test files (`tests/visual/tests/test_*.py`)
invoke `compare_with_contract` post-capture; failure exits
non-zero which propagates through `lib/runner.py` to ctest
which propagates to CI.

On CI failure:

1. `visual-screenshots-<preset>` artifact uploads
   `tests/screenshots/**` (existing S5 step) — operator
   downloads + reviews.
2. Each failing state has a `.diff-report.json` + `.diff.png`
   sibling for forensic review.
3. Env-contract violation (invalid=True) is HALT trigger H10 —
   surfaces in CI log as "env drift" not "visual regression".

---

## 6. Cross-references

- M16 spec: `docs/milestones/M16-visual-identity-ownership.md`
  §2.1 #2 (this deliverable); §4.1 (algorithm sketch);
  §5.2 (acceptance criteria); §6 H10 / H11 (HALT triggers)
- Manifesto: `docs/v0.3/visual-identity.md` §5 (cross-platform
  determinism as design constraint — this contract operationalises
  it)
- Environment contract:
  `docs/v0.3/rendering-environment-lock.md` (the env-sidecar
  schema this algorithm reads at Step 1)
- S0.5 spike: `docs/v0.3/spike-result.md` (empirical justification
  for tunable defaults)
- Reference inventory: `docs/v0.3/references/README.md`
- V0.2 `compare.py` (M16 extends, not replaces):
  `tests/visual/lib/compare.py`
- M16 S3 implementation target: `tests/visual/lib/compare.py`
  + `tools/lint_qss.py`
