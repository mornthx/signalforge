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
