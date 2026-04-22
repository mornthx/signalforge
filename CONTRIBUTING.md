# Contributing

SignalForge is driven by the rules in `CLAUDE.md` at the repository
root — that file is the hard contract for both humans and AI agents
working on this codebase. Read it before opening a pull request.

The full AI-execution protocol (milestone workflow, HALT handling,
commit discipline) lives in `docs/claude-code/execution-manual.md`.

## Quick expectations

- Work happens on `milestone/<id>` branches; `main` is protected and
  merged only via pull request.
- Every commit must build under the `debug`, `release`, and
  `debug-asan` presets, and `ctest` must pass on each.
- `clang-format` (config `.clang-format`) and `clang-tidy` (config
  `.clang-tidy`) are authoritative — don't relax them.
- Commit message format: `<module>: <imperative verb> <object>`,
  subject ≤ 72 characters.
- No `using namespace` in headers. Only `#pragma once` guards. Logging
  goes through the `SF_LOG_*` macros in `src/observability/logging.hpp`.

See `CLAUDE.md` §Required / §Forbidden for the complete list.
