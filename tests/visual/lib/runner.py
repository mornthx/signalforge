"""M15 S2 — minimal stdlib-only test runner for tests/visual/.

Replaces pytest (which had a brittle CI dep chain via pygments).
Each test file ends with:

    if __name__ == "__main__":
        from lib.runner import run_tests
        run_tests(globals())

Discovers `test_*` callables in module globals, runs each one,
prints PASS/FAIL line, exits non-zero if any failed. ctest reads
the exit code as the pass/fail signal.
"""

from __future__ import annotations

import sys
from typing import Any


def run_tests(module_globals: dict[str, Any]) -> None:
    """Run every callable named `test_*` in module_globals; exit on failure."""
    test_names = sorted(
        name
        for name, value in module_globals.items()
        if name.startswith("test_") and callable(value)
    )

    if not test_names:
        print("(no tests discovered)")
        sys.exit(2)

    failures: list[tuple[str, BaseException]] = []
    for name in test_names:
        fn = module_globals[name]
        try:
            fn()
            print(f"PASS  {name}")
        except AssertionError as exc:
            failures.append((name, exc))
            print(f"FAIL  {name}: {exc}")
        except Exception as exc:  # noqa: BLE001 — broad-catch is the runner's job
            failures.append((name, exc))
            print(f"ERROR {name}: {type(exc).__name__}: {exc}")

    print()
    print(f"summary: {len(test_names) - len(failures)} pass, {len(failures)} fail")
    if failures:
        sys.exit(1)
