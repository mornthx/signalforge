# CC native describe-screenshot prompt template

This is the prompt template CC uses when reading a SignalForge
screenshot via the multimodal Read tool. CC's session is local-only
per M15-concerns C7 (public-repo security): this workflow never
runs in CI; pixel-diff is CI's only visual gate.

---

## When to use

Operator or CC (during V0.2 development / S6 autonomy demo /
V0.3 design iteration) wants a structured description of a
SignalForge GUI screenshot. The PNG path is given; CC reads it
via the Read tool, examines the rendered image, and produces
JSON conforming to `tests/visual/lib/schema.py` `SCHEMA_FIELDS`.

## Prompt template

> You are reading a SignalForge GUI screenshot at `<PATH>`.
>
> SignalForge is a Qt 6.10 desktop application for signal
> visualisation + recording. Its main window has a menu bar
> (Connections / Session / File), a chart toolbar (● Live
> toggle / time-preset combo / + Chart button), a left dock
> panel listing connections, a centre signal-selector tree, a
> right chart-pane area, and a bottom status bar. Replay mode
> additionally shows a Replay toolbar (Play / Pause /
> Step / Seek slider / Speed combo / Exit Replay).
>
> Read the PNG and produce a single JSON object matching this
> schema (keys + value types must match exactly):
>
> ```json
> {
>   "window_state": "<one of: idle, connecting, connected, recording, replay, dialog-open, error, unknown>",
>   "widgets_visible": ["<widget name>", ...],
>   "chart_contents": {
>     "lines_visible": <bool>,
>     "trace_count": <int>,
>     "axis_labels_visible": <bool>,
>     "primary_color": "<color name or hex or null>"
>   },
>   "connections": [
>     {"id": "<connection id>", "state": "<state name>"},
>     ...
>   ],
>   "status_bar_text": "<exact text or empty string>",
>   "errors_visible": ["<error message>", ...],
>   "dialogs_open": ["<dialog name>", ...],
>   "menu_open": "<menu name or null>"
> }
> ```
>
> Rules:
>
> 1. Output ONLY the JSON object. No prose before or after.
> 2. If a field is uncertain, use a conservative default
>    (`null` / empty list / `"unknown"` for `window_state`).
>    DO NOT hallucinate.
> 3. Widget names should match the schema's known widgets:
>    `menu_bar`, `chart_toolbar`, `connection_panel`,
>    `signal_selector`, `chart_pane`, `status_bar`,
>    `replay_toolbar`. Add unknown ones as-is if visible.
> 4. `status_bar_text` should be the exact text observed,
>    including any double-percent artefacts (e.g.
>    `"buffer 0%% (0 MiB)"` if rendered that way).
> 5. `chart_contents.trace_count` is the count of distinct
>    coloured signal traces visible in the chart pane (0 if
>    only a default background is rendered).

## Verification

After describing, run:

```python
from tests.visual.lib.validate_description import assert_valid
import json
desc = json.loads(your_output)
assert_valid(desc)  # raises AssertionError if any violation
```

## Why this is local-only

Per M15-concerns C7: SignalForge is a public repo. API key
leakage in CI is unacceptable. CC's Read tool runs in CC's
session; no API key in the application layer; no remote call
from CI. The optional MiMo benchmark
(`tests/visual/lib/describe_via_mimo.py`) similarly uses an
operator-local API key and is never invoked from CI.
