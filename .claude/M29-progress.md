# M29 — progress (Phase A)

## S1 — persistent per-signal intent + symmetric lifecycle  ✅
- `Dashboard` gained `signalIntent_` (`signalId → PanelConfig` template; type+format, no geometry).
- `addSignal` restores the remembered form (else suggests + records first-time). (Report 2.)
- `removeSignalEverywhere` now removes a multi-signal panel once its last signal leaves. (Report 1.)
- `setPanelType` / `setPanelSignals` write the chosen form back into intent so it survives demote→promote.
- Tests: re-check restores Gauge (not Numeric); unchecking a 2-signal plot's last signal removes it.
- Verified: dashboard_test 63/63 Debug + Release; clang-format clean.

## S2 — bounded free+push layout  ⏳ (next)

## Report 4 (raw-data-first) — carried to Phase B/C (Parsed/Raw tabs).
