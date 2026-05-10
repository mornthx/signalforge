"""M15 S2 — top-level describe_screenshot() + backend dispatcher.

Visual tests call `describe_screenshot(path)` which returns either:

- a structured description dict (validated against schema), OR
- `None` if no vision-LLM backend is available (CI default)

Tests then assert on description content when it exists; pixel-diff
in `compare.py` is the always-on gate.

Backend selection (per M15-concerns C2 / C7):

1. If `SF_VISUAL_DESCRIBE_BACKEND=mimo` AND `MIMO_API_KEY` is set
   → call `describe_via_mimo()`. **Operator-local only.**
2. Otherwise → return `None` (CI default; CC's interactive workflow
   uses Read tool manually per `prompts/cc_native_describe.md`).

Never reaches a remote endpoint without explicit operator opt-in.
"""

from __future__ import annotations

import os
from pathlib import Path

from .describe_via_mimo import MimoUnavailable, describe_via_mimo
from .validate_description import validate_description


def describe_screenshot(image_path: Path | str) -> dict | None:
    """Return a structured description of a SignalForge screenshot.

    Returns:
        - dict matching schema.SCHEMA_FIELDS when a vision-LLM
          backend ran successfully
        - `None` when no backend is configured (CI default)

    Operator-local opt-in via `SF_VISUAL_DESCRIBE_BACKEND=mimo` +
    `MIMO_API_KEY` env vars. CI never sets these per M15-concerns
    C7 public-repo security.

    Raises only on schema-validation failures of a backend's
    output (programming error / prompt drift); transient / config
    issues return None.
    """
    backend = os.environ.get("SF_VISUAL_DESCRIBE_BACKEND", "").strip().lower()
    if backend != "mimo":
        # CC native is the developer's interactive path (see
        # prompts/cc_native_describe.md); not callable from this
        # function. CI default: return None → semantic check
        # skipped, pixel-diff continues to gate.
        return None

    try:
        desc = describe_via_mimo(image_path)
    except MimoUnavailable:
        return None

    # Validate before returning so callers can rely on schema.
    violations = validate_description(desc)
    if violations:
        bullet = "\n  - "
        raise AssertionError(
            "MiMo description failed schema validation:" f"{bullet}{bullet.join(violations)}"
        )
    return desc
