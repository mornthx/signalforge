# M36 — Progress (video tooling)

Branch: `milestone/M36` (local, stacked on `milestone/M35`). All commits local; remote ops deferred.

## P0 — Pause / freeze frame — ✅ `0be82e9`
`VideoView::setFrozen` ignores `setFrame`; `VideoPage` Pause/Resume button freezes the display for
inspection while the receiver runs and recording stays live; reset on stream transitions.

## P1 — Zoom + pixel probe — ✅ `2f0fba1`
`VideoView` transform-based view: wheel zoom toward cursor (fit..16×), drag-pan, double-click reset,
hover pixel probe → `pixelProbed(QPoint, QColor)`; `VideoPage` shows the `(x,y) RGB(r,g,b)` readout.

## P2 — Software color correction — ✅ `6ba36d2`
`ColorParams` + `ColorCorrector` (per-channel LUTs + saturation pass; identity = passthrough);
`ColorPanel` sliders; `VideoPage` applies it to display + screenshot, re-renders on param change
(live even while paused). Bench: 87.5 fps / 11.4 ms/frame @ 1280×720 full correction (Release).

## P3 — Color preset save/load (JSON) — ✅ `117f62b`
`colorParamsToJson` / `colorParamsFromJson` (nlohmann/json; missing→default, malformed→nullopt,
logged); `ColorPanel` Save…/Load…; `VideoPage::saveColorPreset` / `loadColorPreset` seams.

## P4 — Recording raw/corrected linkage — ✅ `704ae75`
Record-source combo (Corrected default / Raw uncorrected); `VideoPage` feeds the recorder the chosen
frame; selector disabled while recording.

## P5 — closure — ✅ LOCAL-READY
Full suite **795/795 on Debug and Release** (`ctest -LE visual`, offscreen). `video_test`: 45 cases /
174 assertions. `.claude/M36-done.md` written. **Stop + request authorization for all deferred remote
operations across M35 + M36.**
