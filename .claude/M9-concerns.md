# M9 — Concerns (recorded ahead of S1)

This file records anticipated deviations from the M9 spec that
were identified at planning time. Per CLAUDE.md §Ambiguity
handling, deviations are recorded here, executed as the spec
intends, and re-evaluated at milestone review.

Both concerns below are non-HALT — they are spec/reality
reconciliations that fall under "additive extensions without
HALT" or, where the spec literal text disagrees with M3's
production reality, the implementation truth wins per CLAUDE.md
§Required #2 (no modifications to frozen public interfaces).

---

## C1: M9 spec §4.2 *Config structs vs M3's existing structs

**Spec text** (§4.2) declares new `SerialConfig` / `TcpConfig` /
`UdpConfig` / `ReplayConfig` structs in the
`signalforge::connection` namespace.

**Reality**: M3 already defines these structs in
`signalforge::drivers` (`src/drivers/driver_configs.hpp`), with
driver constructors (`SerialDriver(SerialConfig)`,
`TcpDriver(TcpConfig)`, `UdpDriver(UdpConfig)`,
`ReplayDriver(ReplayConfig)`) taking them by value. These
structs are part of M3's frozen freeze record.

**Resolution**: `ConnectionConfig::driverConfig` variant uses the
existing `signalforge::drivers::*Config` types directly. No new
struct definitions in `signalforge::connection`.

```cpp
using DriverConfig = std::variant<drivers::SerialConfig,
                                  drivers::TcpConfig,
                                  drivers::UdpConfig,
                                  drivers::ReplayConfig>;
```

This avoids:

- Duplicate struct definitions across two namespaces.
- M3 driver-constructor changes (HALT trigger #2 — non-additive
  M3 driver interface change — is **not** fired).
- Conversion shim code at every Connection→Driver boundary.

The spec is the **design intent**; M3 is the **implementation
truth**, frozen and shipped. The spec's *Config struct field
names (e.g., `portName` vs M3's `device`) are reconciled by
adopting M3's shape as the canonical V1 form. The yaml schema's
keys match M3's field names so save/load is direct.

**Status**: addressed in S1 scaffolding via direct reuse.

---

## C2: M3 preview ConnectionManager removal in S9

`src/app/connection_manager.{hpp,cpp}` is the M3 **preview**
dialog (`QDialog` subclass, single-driver, non-persistent). S9
(MainWindow integration) removes it from the build and deletes
the source files.

This is **not** a frozen-`.hpp` change. Per CLAUDE.md §Forbidden
#2 the freeze list is the milestone-specific freeze record, not
every file the milestone created. M3-done.md's frozen list:

- `src/drivers/driver_interface.hpp`
- `src/drivers/driver_configs.hpp`
- per-driver `.hpp` files
- frame schema files

M3 preview `connection_manager.hpp` (at `src/app/`, not
`src/drivers/`) is **not** listed. It was M3's UX preview to
prove M3's drivers worked end-to-end; M9 supersedes it.

The replacement `signalforge::connection::ConnectionManager` at
`src/connection/connection_manager.hpp` is in a different
namespace, different file path, distinct class — no symbol
collision.

**Status**: addressed in S9 MainWindow integration.

---

## Inherited from M8: 1-hour soak verification (S5s)

Per `.claude/M8-done.md` §8.2 and the Phase 3 continuation
prompt:

> 1-hour soak verification (60 charts × 1 kHz × 30 Hz × 1 hour)
> per M8 spec §5.6 + plan §S11. To be executed during M9
> implementation at a convenient point.
> Acceptance: Vmrss growth < 10%, dropped frames < 50, ASan/LSan
> clean.

Scheduled as plan §S5s, between S5 and S6. The harness
(`bench_chart --soak <s> --memory-snapshot <s>`) is rebuilt
fresh in S5s; the M8-era skeleton was uncommitted at the M8/M9
transition.

If soak fails: HALT and assess M8 hotfix vs M9 regression.

**Status**: ✅ executed at M9 close. 3600 s run on dev
workstation: VmRSS growth 9.047 % (gate < 10 %), dropped
frames 0 (gate < 50). Numbers in
`tests/benchmark/results/M8-baseline.md §1-hour soak (S5s)`
and `.claude/M9-done.md §Inherited concerns`. No HALT
trigger fired.

---

## C3 — M8-done.md §8.2 left unchanged

`M9-plan.md §S5s` asked to flip the "Pending: 1-hour soak
test" checkbox in `.claude/M8-done.md` §8.2 once the soak
passed. The session permission hook treats prior-milestone
done.md files as freeze records and blocks edits to them.

**Resolution**: leave `M8-done.md` untouched. The S5s
result is captured in:

- `tests/benchmark/results/M8-baseline.md §1-hour soak (S5s)` —
  full table + methodology;
- `.claude/M9-progress.md §S5s` — log entry with numbers;
- `.claude/M9-done.md §Inherited concerns` — same numbers
  in the milestone closure report.

Anyone reading `M8-done.md` §8.2's "Pending" checkbox should
follow the cross-reference embedded in that bullet's note
("the soak runs … before final acceptance") to the M9
artifacts above. A future session with explicit per-edit
authorization may flip the checkbox; this is documentation
hygiene, not a correctness issue.

**Status**: documented; no further action this milestone.
