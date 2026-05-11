"""M15 S2 — validate a structured GUI description against the schema.

Used by tests + describe_via_mimo.py. Stdlib-only. Returns a list
of human-readable violations; empty list means valid.
"""

from __future__ import annotations

from typing import Any

from .schema import SCHEMA_FIELDS


def validate_description(desc: Any) -> list[str]:
    """Return a list of schema violations; empty if valid.

    Each violation is a single-line string suitable for assertion
    failure messages.
    """
    violations: list[str] = []
    if not isinstance(desc, dict):
        return [f"top-level must be dict; got {type(desc).__name__}"]

    expected_keys = set(SCHEMA_FIELDS.keys())
    actual_keys = set(desc.keys())
    for missing in expected_keys - actual_keys:
        violations.append(f"missing required field: {missing}")
    for extra in actual_keys - expected_keys:
        violations.append(f"unexpected extra field: {extra}")

    for key, (description, predicate) in SCHEMA_FIELDS.items():
        if key not in desc:
            continue  # already reported as missing
        if not predicate(desc[key]):
            violations.append(
                f"field {key!r} fails type / value check ({description}); got {desc[key]!r}"
            )
    return violations


def assert_valid(desc: Any) -> None:
    """Raise AssertionError listing all violations if any."""
    violations = validate_description(desc)
    if violations:
        bullet = "\n  - "
        raise AssertionError(
            f"description failed schema validation:{bullet}{bullet.join(violations)}"
        )
