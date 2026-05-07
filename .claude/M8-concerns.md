# M8 — Concerns

Cumulative log of deviations / open questions / spec-vs-reality
gaps surfaced during M8 implementation.

## C1 — `ChartConfig::signals` field rename to `signalConfigs` (S1)

**Spec text** (§4.1):
```cpp
struct ChartConfig {
    QString id;
    QString title;
    std::vector<ChartSignalConfig> signals;
    std::optional<QString> timeAxisId;
};
```

The field name `signals` collides with Qt's `signals` macro
(expanded by AUTOMOC even in non-`Q_OBJECT` contexts inside a
TU that includes `<QObject>`). Build fails with:

```
src/chart/chart.hpp:46:1: error: Parse error at "signals"
```

This is the same Qt-keyword / member-name collision class that
M5 (signals parameter), M6 (emit lambda), and M7 (slots field)
encountered. Each milestone resolved by renaming the C++
identifier; the spec/user-facing-yaml/etc. wording was kept
intact where applicable.

**Resolution**: rename the C++ field to `signalConfigs`
(verb-less plural, parallels `chartIds`). The yaml key stays
`signals` since that is the user-facing name in
`charts.yaml` and is unaffected by C++ macro expansion.
Per CLAUDE.md §Ambiguity-handling "additive extensions" rule,
this is a structural-not-semantic change applied uniformly across
the freeze surface; documented here per the same rule's "document
in done.md / concerns.md" requirement.

The freeze surface (spec §6.1) records a sha256 of `chart.hpp`;
the field rename is part of what's frozen. Tests + persistence
yaml round-trip use the new C++ name internally.

## C2 — `SignalSelector` cannot observe registry Qt signals (S6)

**Plan §S6 / spec §2.1-4 / §9 closing note**:

> Observes `signalsRegistered` / `signalsUnregistered` Qt signals
> from the registry to keep the tree in sync as decoders register
> or unregister signals mid-session.

**Reality**: `signalforge::buffer::SignalBufferRegistry` is **not**
a `QObject` — it inherits from `signalforge::decoder::SignalValueSink`
(M6 spec §4.2). It exposes `onSignalsRegistered` / `onSignalsUnregistered`
**virtual methods** (called by decoders via the M5 `DecoderRegistrar`),
not Qt signals.

Adding `Q_OBJECT` + emitted signals to the registry would modify
the M6-frozen `signal_buffer_registry.hpp`, which fires HALT
trigger #1 (modification to M2-M7 frozen `.hpp`).

**Resolution**: SignalSelector exposes a public `refresh()` slot.
The expected M8 wiring (S8 MainWindow integration) is:

- `DecoderRegistrar` registers signals → calls
  `registry.onSignalsRegistered(driver, metas)` (existing M6 path).
- The driver-startup callback in MainWindow (S8) invokes
  `selector->refresh()` after each driver finishes its
  `onSignalsRegistered`.

Tests call `selector.refresh()` directly after seeding the
registry. This preserves the user-visible behavior ("the tree
updates when signals appear / disappear") without modifying any
frozen interface.

The plan's S6 deliverable is met with this minor adaptation.
S8 MainWindow integration (next subtask after this concern is
written) will wire the refresh hook into the driver-startup
flow.
