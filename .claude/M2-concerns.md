# M2 — Concerns

This file records concerns, deviations, and locally-blocked verifications
raised during M2 execution.

## Local debug-asan host block

**Status**: known, pre-existing from M0/M1.

The development host has `/etc/ld.so.preload` configured such that the
AddressSanitizer runtime cannot be loaded first. Running the `debug-asan`
preset's test binaries locally produces:

```
==NNN==ASan runtime does not come first in initial library list; you
should either link runtime to your application or manually preload it
with LD_PRELOAD.
```

Per CLAUDE.md §Required-2:

> AddressSanitizer and UBSan violations: verified on the `debug-asan`
> preset when the local host permits; otherwise CI is the authoritative
> gate (document the local block in `.claude/M<n>-concerns.md`).

**Decision**: the `debug-asan` **build** passes locally (all M2 code
compiles clean under `-fsanitize=address,undefined`). Only the **runtime
test execution** is blocked. CI runs the `debug-asan` test pass and is
authoritative for ASan/UBSan violations.

Applies to every M2 subtask commit. Will not be re-reported per-subtask.

---

## Spec §2.2-4 "patch 5" errata — resolved by 2026-04-23 clarification

Superseded by the nine M2 spec clarifications (commit 24e089b) merged into
milestone/M2 at 6e1a2b5. Clarification 9 replaced "patch 5" with "§4.6".

---

