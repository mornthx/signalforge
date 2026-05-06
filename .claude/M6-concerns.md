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

## Concern #3 — `SignalBuffer::TypedBuffer` forward-decl moved to public

**Subtask**: S2.

**Observation**: M6 spec §4.1 declares `struct TypedBuffer;` inside
the `private:` section of `SignalBuffer`:

```cpp
private:
    // Internal: per-variant typed buffer (polymorphic; not in public API)
    struct TypedBuffer;
    std::unique_ptr<TypedBuffer> impl_;
```

Per S2's design — four per-variant implementations
(`BoolTypedBuffer`, `Int64TypedBuffer`, `DoubleTypedBuffer`,
`StringTypedBuffer`) defined in the .cpp's anonymous namespace —
those derived classes need to name `SignalBuffer::TypedBuffer` as a
base class. C++ access rules forbid that when the nested type is
declared `private:` (the compiler errors with "is private within
this context").

**Resolution**: Move the forward declaration to `public:` while
keeping `std::unique_ptr<TypedBuffer> impl_;` private:

```cpp
public:
    struct TypedBuffer;  // forward declaration only

private:
    std::unique_ptr<TypedBuffer> impl_;
```

External code still cannot construct or use `TypedBuffer` because
the full definition lives only in `signal_buffer.cpp`. The change
makes the type-name accessible (so the per-type implementations in
the same TU can inherit) but exposes no functionality.

**Why this is permitted**: M6 spec §6.2 explicitly states "TypedBuffer
polymorphism (internal to .cpp; may evolve)" is outside the M6
freeze surface. The freeze surface §6.1 enumerates SignalBuffer's
public methods + the named structs (`SignalSample`, `LatestValue`,
`SignalBufferConfig`); `TypedBuffer` is none of those. Therefore
the access-level placement is implementation detail, not a freeze
violation.

**Status**: Informational. No spec amendment, no API change visible
to consumers, no new dependency.

---

## Concern #4 — `std::deque` storage vs spec §3.2 "ring-buffer-style" wording

**Subtask**: S3.

**Observation**: M6 spec §3.2 states "Eviction model: ring-buffer-style.
Writer monotonically advances head; older indices are reused
(overwriting evicted samples)." A literal reading would imply a
preallocated circular buffer of `capSamples` entries with modular head
indexing.

**Resolution**: S3 implements eviction via `std::deque<T>` with O(1)
front pop instead of a preallocated ring buffer. Functionally, both
maintain a sliding-window time series; the difference is allocation
strategy:

- Ring buffer (literal spec): preallocate `capSamples` entries
  upfront; head and tail indices wrap modulo capacity.
- `std::deque` (this implementation): grow on demand; pop_front
  releases the front chunk when empty.

Why std::deque was chosen:
- `capSamples` defaults to 1 000 000 (a safety cap, not typical
  sizing). Preallocating that for QString = 24 MB / signal × 60+
  signals = > 1.4 GB before any data lands.
- Typical usage stores ~60 000 samples (60 s × 1 kHz). Pre-allocating
  the cap wastes memory by a factor of 16 in steady state.
- O(1) front pop matches what the spec needs (cheap eviction).
- The S4 snapshot publish step copies the deque into a contiguous
  `std::vector` for the immutable segment; readers see contiguous
  data.

The spec §3.2 wording describes intent (sliding-window with eviction)
rather than mandating a specific data structure. Performance is
verified at S11; if benchmarks miss the writer-throughput targets,
a true ring buffer or a custom chunked structure can be substituted
behind the unchanged `TypedBuffer` interface (per spec §6.2,
TypedBuffer polymorphism is not part of the freeze surface).

**Status**: Informational. No spec amendment, no API change, no new
dependency. Re-evaluate at S11 if performance targets are not met.

---

(Future entries will be appended below as discovered during S4-S12.)
