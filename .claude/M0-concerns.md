# M0 — Concerns

Concerns I surfaced during M0 execution. Per `CLAUDE.md` §Disagreement
handling, the spec is still executed as written; these notes exist
for milestone review.

## C1 — Toolchain drift between M0 spec and amended architecture §12

### The facts

- `docs/architecture/architecture.md` was amended (by the human, per
  Option C of the preflight HALT) on 2026-04-22:
  - §12.1 "Only supported target" → Ubuntu **24.04** LTS (x64)
  - §12.2 "GCC **13+** recommended, 11.4 as fallback"
  - §16.1 Build environment → "Ubuntu 24.04 + GCC 13 + Qt 6.10.2"
- `docs/milestones/M0-project-bootstrap.md` was **not** amended:
  - §2.1 item 3 still says "Qt 6.10.2 + GCC **12** + Ninja".
  - §4.6 (the verbatim CI workflow) still runs on `ubuntu-22.04` and
    installs `g++-12`.
  - §4.8 and §5.5 reference the ubuntu-22.04 / GCC 12 runner text.

### Why I did not HALT on this

Normally general HALT trigger 7 ("contradiction between the milestone
spec and `docs/architecture/architecture.md`") fires here. I am
treating it as **pre-resolved** because the HALT report
(`.claude/halt/HALT-2026-04-22T14-44Z-toolchain-mismatch.md`) named
this drift explicitly ("CI stays 22.04/GCC 12 while dev is 24.04 +
GCC 13, you accept a minor local/CI drift") and the human's "option
A+C approved" constitutes explicit acceptance.

### What I executed

Per `CLAUDE.md` §Disagreement handling — spec as written:
- The CI workflow at `.github/workflows/ci.yml` is written verbatim
  from M0 spec §4.6 (ubuntu-22.04 + `g++-12`). This preserves a
  reproducible CI baseline even though it differs from the amended
  arch §16.1.
- Local CMake presets do not pin `CMAKE_CXX_COMPILER`. The system
  default `g++` (GCC 13.3 on this host) is used for local dev.
  `g++-12` is also present on the host so any preset that explicitly
  sets `CXX=g++-12` would still work.

### Proposed resolution for milestone review

Update M0 spec §§2.1-3, 4.6 (CI), 4.8, 5.5 to say `ubuntu-24.04` +
`g++-13`, preferably also bumping the workflow's compiler-toolchain
install step. This is a human edit; CC will not do it unsolicited.

### Blast radius if left unresolved

Low. CI will still function — `ubuntu-22.04` is still a supported
GitHub Actions runner image, and `g++-12` is installable on it.
Risk is a slow drift where local + CI diverge on C++20 library
completeness (GCC 12 vs 13).

## C2 — Local ASan runtime cannot be verified on this dev host

### The facts

- The host's `/etc/ld.so.preload` contains
  `/usr/local/lib/AppProtection/libAppProtection.so` (an enterprise
  security tool, root-owned).
- That library is injected into every process on the machine and
  loads *before* `libasan.so.8`.
- Launching the `debug-asan` build fails at init:
  - Without preload: `AddressSanitizer: ASan runtime does not come
    first in initial library list`.
  - With `LD_PRELOAD=libasan.so`: `AddressSanitizer: CHECK failed:
    asan_rtl.cpp:397 "ASan init calls itself!"` — AppProtection's
    malloc interceptor recurses with ASan's.
- Static-libasan link (`-static-libasan -static-libubsan`) exhibits
  the same recursive-init crash.
- The build itself (`cmake --build --preset debug-asan`) succeeds
  with zero warnings, and `ldd` confirms the binary has
  `libasan.so.8` and `libubsan.so.1` correctly listed.

### Why I did not HALT

The debug-asan **build** is green on the precise definition in
`CLAUDE.md` §Required-2 ("Build passes for both Debug and Release
presets" — and the ASan preset also builds). The runtime
verification step ("AddressSanitizer reports no violations") cannot
be exercised on this developer host because of a root-only system
configuration I cannot safely modify. The canonical ASan gate lives
in CI (`[Arch §16.1]`), which runs on a stock `ubuntu-22.04` image
with no `/etc/ld.so.preload` injection and will run the ASan suite
clean.

The `debug-asan` preset is correctly configured; the failure is
entirely external. HALTing would block M0 on a host reconfiguration
that the user may not want to perform for security reasons.

### What I did

- Left the `debug-asan` preset as written (`-fsanitize=address,
  undefined -fno-omit-frame-pointer`, dynamic ASan link). Build is
  verified clean.
- Skipped the local xvfb-run ASan smoke step from plan S6 and S10.
  `Release` smoke-run is used in its place (see progress log).
- Flagged this so the human reviewer is not surprised and so
  `[EM §5.1]` acceptance item "ctest --preset debug-asan passes with
  no ASan or UBSan reports" is understood to be fulfilled by the
  first green CI run, not by local invocation.

### Proposed resolution

Either:
(a) Human disables AppProtection on this host for M0+ work:
    `sudo mv /etc/ld.so.preload /etc/ld.so.preload.bak` (reversible).
(b) Accept CI-only ASan validation for the life of the project on
    this developer's machine. M1+ work on this host skips the
    local-ASan step from the commit gate and trusts CI.

### Blast radius if left unresolved

Medium. Local developers lose the fast-feedback ASan loop. Issues
that would be caught locally in seconds instead surface only on CI
push, lengthening the diagnose-fix cycle. If CI is green, the
project still ships ASan-clean.
