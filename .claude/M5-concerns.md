# M5 — Concerns and deviations

Documented inline as discovered per M5 Phase 5 session instructions.
Schema v1 ambiguities get entries here the moment they surface; C++
deviations similarly.

---

## 1. `SignalValueSink::onSignalsRegistered` parameter renamed `signals` → `signalsList`

**When**: S1, during the C++23 toolchain bump build verification.

**What the spec says** (M5 §4.1):

```cpp
virtual void onSignalsRegistered(const QString& driverId,
                                 const std::vector<SignalMetadata>& signals) {
```

**What broke**: GCC 13 `-std=c++23` compile error
`expected ',' or '...' before 'public'` at the parameter declaration.
Cause: Qt 6 defines `signals` as a preprocessor macro that expands
to `public` under default `QT_KEYWORDS` mode (needed for moc to see
`signals:` access-specifier blocks in QObject classes). Including
`<QString>` transitively defines the macro; `SignalValueSink` is pure
C++ but the macro still fires on the identifier.

**Resolution**: rename the parameter `signals` → `signalsList` in the
three locations:
- `src/decode/decoder_interface.hpp` (the frozen interface)
- `src/decode/logging_signal_value_sink.hpp`
- `src/decode/logging_signal_value_sink.cpp`

**Impact**:
- Parameter names are not part of the ABI (the virtual's signature is
  `void(QString const&, std::vector<SignalMetadata> const&)` either
  way), so binary compatibility is unaffected.
- Doxygen-visible API changes slightly: users reading the interface
  see `signalsList` instead of `signals`. Acceptable — the name is
  strictly more descriptive (the parameter is a list/catalog of
  signal metadata, not Qt signals).
- Alternative considered: add `QT_NO_KEYWORDS` definition to
  `signalforge_decoder` and require consumers of this header to use
  `Q_SIGNALS` / `Q_SLOTS` / `Q_EMIT`. Rejected — would propagate
  across all consumers of the decoder module and conflict with
  existing code that uses `signals:` access specifier blocks.

**Freeze implication**: the frozen interface signature (types only) is
identical to spec §4.1. The parameter *name* in the freeze record
(`M5-done.md` SHA256) reflects `signalsList`; a consumer reading the
spec should see the note in `decoder_interface.hpp` explaining the
rename.

**Status**: resolved in S1. Needs human acknowledgment at milestone
review — if the name must match the spec exactly, the `QT_NO_KEYWORDS`
route is available at the cost of a broader surface change.
