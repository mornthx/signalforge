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
