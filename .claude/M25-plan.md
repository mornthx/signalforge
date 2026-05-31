# M25 — C1 fix (plan)

Local commits on `milestone/M25` (no push). Single focused change + test.

## S1 — Time-based publish flush

- `signal_buffer.cpp`: add `constexpr auto kPublishFlushInterval = std::chrono::milliseconds(200);`
  near `kDefaultPublishCadence`. In the TypedBuffer base, add members
  `std::chrono::steady_clock::time_point lastPublishTime_{}; bool publishAnchorValid_ = false;`.
  In the push publish-trigger (step 7): lazily anchor `lastPublishTime_ = t` on first push;
  publish when `(++pushesSincePublish_ >= publishCadence_) || (t - lastPublishTime_ >=
  kPublishFlushInterval)`; on publish reset `pushesSincePublish_ = 0` and `lastPublishTime_ = t`.
- Test `tests/unit/buffer/signal_buffer_time_flush_test.cpp`:
  - slow: push @t0 (not visible), push @t0+250 ms → `queryLatestOne` now has the value.
  - fast cluster: 99 pushes µs apart not visible; 100th publishes (cadence, time flush silent).
- Run the full buffer test suite to confirm no cadence regressions.
- Commit: `buffer: add time-based publish flush for slow signals`

## S2 — Close-out

- Full Debug + Release ctest; clang-format; update `.claude/M25-progress.md`, `M25-done.md`;
  mark C1 resolved in `.claude/M21-concerns.md`.
- Commit: `buffer: M25 C1 close-out`

## Risks / HALT

- Internal-only change (TypedBuffer outside M6 freeze). If a publish-cadence test regresses
  unexpectedly and can't be reconciled with the 200 ms interval after analysis → re-evaluate
  the interval; do not loosen test assertions. Compile/test fail ×3 → HALT.
