# M36 — Done report (video tooling)

**Milestone:** M36 — pause/freeze, zoom + pixel probe, software color correction (standard set) +
JSON presets, recording raw/corrected linkage. Stacked locally on M35.

## Status: LOCAL-READY — remote git ops deferred (awaiting per-op authorization)
All M35 **and** M36 work is committed locally (`milestone/M35`, then `milestone/M36` branched from it).
**Nothing pushed; no PRs; no merges; no tags.** Per the session `/goal` ("work through M36, all choices
yours, full records + commits for review and rollback") I executed both milestones locally; per CLAUDE.md
§4, `git push` / `gh pr create` / `gh pr merge` / tags need **per-operation** authorization (session-level
blanket auth is invalid). This is the stop point: review the stack and authorize the remote operations.

## Scope delivered (P0–P4)
- **P0** `0be82e9` — pause/freeze (display frozen for inspection; receiver + recording stay live).
- **P1** `2f0fba1` — zoom (wheel-toward-cursor, fit..16×, drag-pan, double-click reset) + hover pixel probe.
- **P2** `6ba36d2` — `ColorCorrector` (per-channel LUTs + saturation) + `ColorPanel` sliders; applied to
  display + screenshot; live re-render.
- **P3** `117f62b` — color presets ⇄ JSON (`nlohmann/json`), Save…/Load….
- **P4** `704ae75` — recording source selector (Corrected default / Raw).

## Verification
- **Build:** Debug ✅, Release ✅. ASan → CI gate (local preload block).
- **Tests:** full suite **795/795 on Debug and Release** (`ctest -LE visual`, offscreen). `video_test`
  grew to 45 cases / 174 assertions across protocol, receiver, page, recorder, color correction +
  presets. GUI tests use a leaked QApplication + offscreen (per `memory/qt_xcb_teardown_crash`).
- **Perf (§5):** color correction 87.5 fps / 11.4 ms/frame @ 1280×720 with full correction (Release);
  identity is a free passthrough.
- **clang-format:** clean. **clang-tidy:** only the codebase's accepted idiom categories (Qt
  parent-ownership `new X(this)`, widget-construction member-init, LUT constant-array-index, a local
  data-table) — `WarningsAsErrors: ''`.

## Frozen interfaces / dependencies
None changed. **Zero new link dependencies** — color presets reuse `nlohmann/json` (already §4.1);
everything else is Qt Gui/Widgets. ffmpeg remains a runtime tool (M35).

## Footprint (origin/main...HEAD, **M35 + M36 combined**)
**src/ +2125, tests/ +1222** across 11 code commits (M35 5 + M36 5) + docs. M36 alone: src +709 / test +388.

## Deviations and concerns
- **§4 (≤800 net lines / PR):** the combined branch is ~2125 production lines. M35 alone ~1424, M36
  alone ~709. Precedent: M34 was +21,684 in one PR. **At authorization time**, choose: two PRs
  (M35 then M36, the stacked layout already supports this) or, if preferred, split further. Recommend
  **M35 PR then M36 PR** — the branches are already separated for exactly this.
- **Color correction on the GUI thread:** ~11.4 ms/frame at full correction (Release), comfortably under
  the 40 ms (25 fps) budget and only when correction is non-identity. The deferred per-component-worker
  design (`memory/heterogeneous_frame_rates`) is the natural next optimization if heavier correction or
  higher resolutions arrive; raw frames are kept so a worker offload is a drop-in change.
- **Single video stream**, **bpp≠3 warn-only**, **no live-video visual baselines** — as M35.

## PR / CI / merge / tag
- _None performed — awaiting authorization._ Suggested order once authorized:
  1. push `milestone/M35` → CI green → PR to `main` (M35 video core);
  2. merge M35, tag `v0.0.35.1`;
  3. rebase/retarget `milestone/M36` on updated `main` → push → CI → PR (M36 video tooling);
  4. merge M36, tag `v0.0.36.1`.
  (Or a single combined PR if you prefer, accepting the §4 size like M34.)

## Next
Awaiting per-operation authorization for the remote git operations above. No further local code changes
pending.
