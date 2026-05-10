"""M15 S2 — canonical structured JSON schema for screenshot descriptions.

Single source of truth per M15-concerns C2 / Phase 5 amendment.
Used by:
- `describe_via_mimo.py` (operator-local optional MiMo benchmark)
- `validate_description.py` (schema-conformance check)
- `tests/visual/lib/prompts/cc_native_describe.md` (prompt template
  for CC's interactive Read-tool workflow)

Schema fields per spec §4.2. Stdlib-only — no `pydantic` or
`jsonschema` dep.
"""

from __future__ import annotations

# Allowed values for closed enums
WINDOW_STATES = (
    "idle",
    "connecting",
    "connected",
    "recording",
    "replay",
    "dialog-open",
    "error",
    "unknown",
)

# Required top-level keys with expected Python type predicates.
# Lambda value receives the field; returns True iff the value matches the
# expected schema. Stays simple; no extra dependency.
SCHEMA_FIELDS: dict[str, tuple[str, callable]] = {
    "window_state": (
        "str — one of WINDOW_STATES",
        lambda v: isinstance(v, str) and v in WINDOW_STATES,
    ),
    "widgets_visible": (
        "list[str] — names of widgets visible (toolbar / connection_panel / chart_pane / signal_selector / status_bar / replay_toolbar / menu_bar / …)",
        lambda v: isinstance(v, list) and all(isinstance(x, str) for x in v),
    ),
    "chart_contents": (
        "dict — { lines_visible: bool, trace_count: int, axis_labels_visible: bool, primary_color: str|None }",
        lambda v: isinstance(v, dict)
        and isinstance(v.get("lines_visible"), bool)
        and isinstance(v.get("trace_count"), int)
        and isinstance(v.get("axis_labels_visible"), bool)
        and (v.get("primary_color") is None or isinstance(v.get("primary_color"), str)),
    ),
    "connections": (
        "list[dict] — each { id: str, state: str }",
        lambda v: isinstance(v, list)
        and all(
            isinstance(c, dict)
            and isinstance(c.get("id"), str)
            and isinstance(c.get("state"), str)
            for c in v
        ),
    ),
    "status_bar_text": (
        "str — exact text content of the status bar (or '' if absent)",
        lambda v: isinstance(v, str),
    ),
    "errors_visible": (
        "list[str] — error messages visible (empty list if none)",
        lambda v: isinstance(v, list) and all(isinstance(x, str) for x in v),
    ),
    "dialogs_open": (
        "list[str] — names of modal dialogs currently open (empty list if none)",
        lambda v: isinstance(v, list) and all(isinstance(x, str) for x in v),
    ),
    "menu_open": (
        "str | None — name of the open menu (Connections / Session / File) or None",
        lambda v: v is None or isinstance(v, str),
    ),
}


# Reference example used by both prompt templates and the validator's
# "what does a valid description look like" docstring.
EXAMPLE_DESCRIPTION = {
    "window_state": "connected",
    "widgets_visible": [
        "menu_bar",
        "chart_toolbar",
        "connection_panel",
        "signal_selector",
        "chart_pane",
        "status_bar",
    ],
    "chart_contents": {
        "lines_visible": False,
        "trace_count": 0,
        "axis_labels_visible": False,
        "primary_color": None,
    },
    "connections": [
        {"id": "M14 smoke UDP [UDP]", "state": "Connected"},
    ],
    "status_bar_text": "FPS: ~30 / chart Dropped: 0 buffer 0% (0 MiB) 1/1 connected Idle",
    "errors_visible": [],
    "dialogs_open": [],
    "menu_open": None,
}
