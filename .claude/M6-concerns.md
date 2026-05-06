# M6 — Concerns and deviations

This file records spec-vs-implementation deviations and ambiguities
discovered during M6 execution. Each entry follows the M5 format:
identifier, subtask, observation, resolution, status.

Both pre-recorded entries below are informational ("additive
extensions without HALT" per CLAUDE.md §Ambiguity handling): they do
not change the M5 frozen interface, do not introduce new
dependencies, and do not modify any M2/M3/M4/M5 frozen `.hpp`.

---

## Concern #1 — Spec §4.6 DecoderRegistrar diff inaccuracy

**Subtask**: pre-S1 (will be addressed in S8).

**Observation**: M6 spec §4.6 ("DecoderRegistrar wiring update")
shows a "Before / After" diff implying that M5's `DecoderRegistrar`
constructed `LoggingSignalValueSink` internally:

```cpp
// Spec's "Before" example
DecoderRegistrar(QObject* parent = nullptr) : QObject(parent) {
    sink_ = std::make_shared<LoggingSignalValueSink>();
}
```

The actual M5 implementation already accepts `defaultSink` as a
constructor parameter (see `src/decode/decoder_registrar.hpp:50-52`):

```cpp
DecoderRegistrar(signalforge::pipeline::PipelineManager* manager,
                 std::unordered_map<QString, QString> driverTypeToSchemaPath,
                 std::shared_ptr<SignalValueSink> defaultSink,
                 QObject* parent = nullptr);
```

**Resolution**: M6's wiring change is at the **call site** that
constructs `DecoderRegistrar` — currently in the application
bootstrap (`src/app/` or `src/main.cpp`, exact location to be
located in S8) — not in `decoder_registrar.cpp`. The call site
constructs a process-singleton `SignalBufferRegistry`, then passes
`std::shared_ptr<SignalValueSink>(&registry, [](auto*) {})` (a
non-owning aliased `shared_ptr` because the registry outlives the
registrar) as `defaultSink`. Zero modification to the M5-frozen
`DecoderRegistrar` constructor signature.

**Status**: Informational. Planned to be addressed in S8. Does not
change spec semantics.

---

## Concern #2 — `SignalMetadata::sample_rate_hz` does not exist

**Subtask**: pre-S1 (will be addressed in S7).

**Observation**: M6 spec §3.6 ("Memory budget hard limit") describes
budget estimation using "metadata's `sample_rate_hz` field if
present (e.g., metadata's `sample_rate_hz` field)". The actual
M5-frozen `SignalMetadata` struct (`src/decode/decoder_interface.hpp`,
sha256 `7a28c64d…b085f44`) carries only:

```cpp
struct SignalMetadata {
    QString id;
    QString name;
    QString unit;
    SignalType type;
    std::optional<QString> description;
    std::optional<double> scale;
    std::optional<double> offset;
};
```

There is no `sample_rate_hz` field.

**Resolution**: Budget estimation uses
`SignalBufferConfig::estimatedRateHz` (caller-supplied at registration
time, per spec §4.1) when set; otherwise falls back to the registry
default of 1000 Hz (per spec §3.6's "if unset, registry uses 1000 Hz
default"). No M5 freeze-surface modification needed; the
`SignalBufferConfig::estimatedRateHz` field already declared in the
M6 spec §4.1 is the canonical mechanism.

**Status**: Informational. Built into the S7 (`SignalBufferRegistry`)
design and reflected in plan §S7.

---

(Future entries will be appended below as discovered during S1-S12.)
