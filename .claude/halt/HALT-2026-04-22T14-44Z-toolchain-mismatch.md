# HALT — M0 / preflight

## Trigger

Two distinct triggers fire in preflight, combined here because their
remediation is the same human decision:

1. **M0 §6-2** — "GCC 12 is unavailable and GCC 11 fails to compile
   the required C++20 features". GCC 12 is unavailable locally.
   (GCC 11 is present but has not been exercised yet, so the
   "fails to compile" half of the trigger is not yet confirmed;
   see §Candidate interpretations below.)
2. **General HALT trigger 5** — "You need to modify any file in
   the Forbidden list above", combined with EM §10 Conflict
   Resolution. Spec-level contradictions between the local host
   and `docs/architecture/architecture.md` §12 can only be
   resolved by editing the architecture doc, which `CLAUDE.md`
   §Forbidden-2 forbids CC from doing.

## Context

- Currently executing: preflight toolchain check (plan subtask 0,
  before plan S1).
- Completed earlier in this session:
  - `.claude/M0-understanding.md`, `.claude/M0-plan.md` written
  - `CLAUDE.md` created verbatim from `[EM §2]` per EM §9 step 8,
    committed as `a9aa6dc docs: add CLAUDE.md verbatim from
    execution manual §2`.
- Files modified but not in an acceptable state: none.

## Problem details

Preflight findings against `docs/architecture/architecture.md` §12
and `docs/milestones/M0-project-bootstrap.md` §2.1 item 3:

| Check | Required by spec | Observed | Status |
|---|---|---|---|
| Host OS | Ubuntu 22.04 LTS x64 (`[Arch §12.1]`) | **Ubuntu 24.04.4 LTS (noble)** | ❌ mismatch |
| Qt 6.10.2 at `~/Qt/6.10.2/gcc_64/` | `[Arch §12.3]` | Present at `/home/shuai/Qt/6.10.2/gcc_64/lib/cmake/Qt6` | ✅ ok |
| GCC | 12+ recommended, 11.4 fallback (`[Arch §12.2]`) | GCC **11.5.0** present; GCC **13.3.0** is system default; **GCC 12 not installed** | ⚠️ fallback-only |
| CMake | 3.22+ | 3.28.3 | ✅ ok |
| Ninja | any (`[Arch §4.1]` + M0 §4.2) | 1.11.1 | ✅ ok |
| `clang-format` | required per `CLAUDE.md` §Required-2 | **not installed** | ❌ missing |
| `clang-tidy` | required per `CLAUDE.md` §Required-2 | **not installed** | ❌ missing |

Exact command output (abridged):

```
$ lsb_release -d
Description:    Ubuntu 24.04.4 LTS

$ g++ --version | head -1
g++ (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0

$ g++-12 --version
bash: g++-12: command not found

$ dpkg -l | grep '^ii  g++'
ii  g++       4:13.2.0-7ubuntu1
ii  g++-11    11.5.0-1ubuntu1~24.04.1
ii  g++-13    13.3.0-6ubuntu2~24.04.1

$ clang-format --version
bash: clang-format: command not found

$ apt-cache policy g++-12 clang-format clang-tidy
g++-12       Candidate: 12.4.0-2ubuntu1~24.04.1
clang-format Candidate: 1:18.0-59~exp2
clang-tidy   Candidate: 1:18.0-59~exp2
```

All three missing packages are available via the default Ubuntu 24.04
apt repositories (`noble-updates`, `noble`), so the remediation is
straightforward — but installing system packages is a "modifies
shared or production systems" action that auto-mode rule 5 requires
me to ask about.

## Why the Understanding's preflight plan didn't catch this pre-plan

`.claude/M0-understanding.md` §4 HALT-risk 1 and 2 named the Qt /
GCC / Ninja / clang-format availability checks, but flagged only Qt
as likely to be pre-checked "before writing any CMake". I should
have executed preflight before the Appendix-A plan-writing phase,
not scheduled it as "plan subtask 0". The bootstrap order is now:
user → "read docs" → understanding+plan → §9 step 8 correction → this
HALT. A more defensive reading of §3.3 step 1 ("Read `CLAUDE.md`, the
current milestone spec, and the relevant sections of
`docs/architecture/architecture.md`") would include *verifying the
environment matches the spec before writing the plan*, because the
plan is invalid if the environment cannot execute it.

## Candidate interpretations or approaches

### Option A — apt-install the missing packages on the current host

Commands (need sudo; user confirmation required):

```
sudo apt-get update
sudo apt-get install -y g++-12 clang-format clang-tidy
```

**Implications**:
- Fastest path. ~2 min of downloads.
- Host OS (Ubuntu 24.04) stays non-conformant with `[Arch §12.1]`.
  That mismatch has to be addressed separately (see Option C).
- Adds ~400 MB to the host. Reversible with `sudo apt-get remove`.
- Does not change any project file.

### Option B — use GCC 11 (the declared fallback) without installing GCC 12

**Implications**:
- In-spec per `[Arch §12.2]` ("11.4 as fallback").
- GCC 11.5.0 is newer than the declared fallback (11.4); acceptable.
- Still does not resolve the missing `clang-format` / `clang-tidy`,
  so Option A's apt install of those two is still needed.
- M0 spec §2.1 item 3 literally says "Qt 6.10.2 + GCC 12 + Ninja"
  without mentioning the fallback, creating a wording conflict with
  arch. Per EM §10 this would need an `.claude/M0-overrides.md`
  entry unless the M0 spec is amended.
- CI workflow (M0 spec §4.6) hardcodes `g++-12` — it installs
  GCC 12 itself on the runner. So CI is unaffected by local choice.
  Only local dev would use GCC 11.

### Option C — amend `docs/architecture/architecture.md` §12 to allow Ubuntu 24.04 + GCC 13

**Implications**:
- Cleanest long-term: makes the spec match reality on the developer's
  host.
- Requires a human edit to `docs/architecture/**`, which is in the
  `CLAUDE.md` §Forbidden-2 list. Only a human can do this.
- GCC 13 is strictly newer than GCC 12 and a superset in C++20
  feature completeness; risk is low.
- Affects the CI workflow too: either keep it pinned to 22.04 + GCC 12
  (CI as canonical build; local dev drifts) or update it. If CI
  remains 22.04 + GCC 12 and dev is 24.04 + GCC 13, you accept
  a minor local/CI drift.

### Option D — run M0 inside an Ubuntu 22.04 container / VM

**Implications**:
- Spec-faithful. No architecture-doc edits required.
- Significant setup cost for the developer. Auto mode cannot
  bootstrap this.
- Out of my execution scope — the human would need to set up
  the container and re-invoke CC inside it.

## Decision requested

1. Which option (A, B, C, D, or some combination)? My recommendation
   for the fastest *and* spec-faithful path is **A + C combined**:
   install the three packages now, then the human edits
   `[Arch §12]` to formally permit 24.04/GCC 13 as supported dev
   hosts (CI stays 22.04/GCC 12).
2. If Option A is chosen, do I have authorization to run:
   `sudo apt-get update && sudo apt-get install -y g++-12 clang-format clang-tidy`?
3. If Option B is chosen, please also confirm whether M0 spec §2.1
   item 3's "GCC 12" wording should be amended or treated as
   "GCC 12 recommended, GCC 11 fallback permitted", and whether
   an `.claude/M0-overrides.md` entry is needed.
4. If Option C is chosen, please edit `docs/architecture/architecture.md`
   §12 and I will resume from preflight after.

## Side effects to clean up on resume

- None. No source files have been created or modified yet.
- `CLAUDE.md` (committed separately as §9 step 8) stays — it is
  correct regardless of which option is chosen.
- `.claude/M0-understanding.md` and `.claude/M0-plan.md` are not
  yet committed. They remain valid; plan subtask 0 (preflight)
  needs one-line addenda referencing whichever option is selected.
