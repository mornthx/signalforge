"""M15 S2 — optional MiMo API benchmark wrapper (local-only).

Per M15-concerns C2 + C7 + Phase 5 amendment:

- This is the OPERATOR'S optional cross-reference path. CC's
  primary describe workflow is the multimodal Read tool (see
  `prompts/cc_native_describe.md`).
- This module makes a HTTP call to Xiaomi's MiMo-V2.5 API
  (OpenAI-compatible at `https://api.xiaomimimo.com/v1`,
  model `mimo-v2.5`) when the operator's local environment has
  `MIMO_API_KEY` set.
- **Never invoked from CI.** Public-repo security: no
  `secrets.MIMO_API_KEY` in any GitHub Actions workflow; no
  `.env*` committed; if `MIMO_API_KEY` env var is absent, this
  module's `describe_via_mimo()` returns `None` and the test
  caller falls back to "skip semantic check; pixel-diff
  remains the CI gate".

Stdlib-only (uses `urllib.request` + `base64` + `json`); no
new top-level dep.
"""

from __future__ import annotations

import base64
import json
import os
import urllib.error
import urllib.request
from pathlib import Path

from .schema import EXAMPLE_DESCRIPTION, SCHEMA_FIELDS


# Defaults; operator may override via env
DEFAULT_ENDPOINT = "https://api.xiaomimimo.com/v1/chat/completions"
DEFAULT_MODEL = "mimo-v2.5"
DEFAULT_TIMEOUT_S = 60


class MimoUnavailable(RuntimeError):
    """Raised when MiMo API call cannot be made (no key, network, etc).

    Caller should treat this as "skip semantic check"; pixel-diff
    remains the visual gate.
    """


def _build_prompt() -> str:
    """The describe-screenshot prompt sent to MiMo.

    Mirrors `prompts/cc_native_describe.md` so CC native + MiMo
    benchmark produce the same schema. Embeds the schema field
    list inline so the prompt is self-contained.
    """
    field_lines = "\n".join(f"  - {key}: {desc}" for key, (desc, _) in SCHEMA_FIELDS.items())
    example_json = json.dumps(EXAMPLE_DESCRIPTION, indent=2)
    return (
        "You are reading a SignalForge GUI screenshot. SignalForge is a "
        "Qt 6.10 desktop application for signal visualisation + recording. "
        "Main window: menu bar (Connections / Session / File), chart "
        "toolbar (● Live / time-preset combo / + Chart), left connections "
        "dock, centre signal-selector tree, right chart-pane area, bottom "
        "status bar. Replay mode adds a Replay toolbar (Play / Pause / "
        "Step / Seek slider / Speed / Exit).\n\n"
        "Output ONLY a single JSON object with these fields:\n"
        f"{field_lines}\n\n"
        "Reference example (a connected-state baseline):\n"
        f"{example_json}\n\n"
        "Rules: (1) Output ONLY the JSON object, no prose. (2) If a "
        "field is uncertain, use null / empty list / 'unknown'. DO NOT "
        "hallucinate. (3) status_bar_text must be the exact text shown. "
        "(4) trace_count counts distinct coloured signal traces visible "
        "in the chart pane (0 if only background)."
    )


def describe_via_mimo(
    image_path: Path | str,
    *,
    endpoint: str | None = None,
    model: str | None = None,
    timeout_s: float = DEFAULT_TIMEOUT_S,
) -> dict:
    """Describe a SignalForge GUI screenshot via the MiMo API.

    Returns a dict matching `schema.SCHEMA_FIELDS`. Raises
    `MimoUnavailable` if the API key is absent or the call fails.

    Operator-local; never called from CI.
    """
    api_key = os.environ.get("MIMO_API_KEY")
    if not api_key:
        raise MimoUnavailable(
            "MIMO_API_KEY env var not set; describe_via_mimo skipped. "
            "(This is expected in CI per M15-concerns C7.)"
        )

    image_path = Path(image_path)
    if not image_path.is_file():
        raise MimoUnavailable(f"image file not found: {image_path}")

    image_b64 = base64.b64encode(image_path.read_bytes()).decode("ascii")

    body = {
        "model": model or DEFAULT_MODEL,
        "messages": [
            {
                "role": "user",
                "content": [
                    {"type": "text", "text": _build_prompt()},
                    {
                        "type": "image_url",
                        "image_url": {
                            "url": f"data:image/png;base64,{image_b64}",
                        },
                    },
                ],
            }
        ],
        "temperature": 0.0,
        "response_format": {"type": "json_object"},
    }

    req = urllib.request.Request(
        endpoint or DEFAULT_ENDPOINT,
        data=json.dumps(body).encode("utf-8"),
        headers={
            "Content-Type": "application/json",
            "Authorization": f"Bearer {api_key}",
        },
        method="POST",
    )

    try:
        with urllib.request.urlopen(req, timeout=timeout_s) as resp:
            payload = json.loads(resp.read().decode("utf-8"))
    except urllib.error.HTTPError as e:
        raise MimoUnavailable(f"MiMo API HTTP {e.code}: {e.reason}") from e
    except urllib.error.URLError as e:
        raise MimoUnavailable(f"MiMo API network error: {e.reason}") from e
    except (TimeoutError, OSError) as e:
        raise MimoUnavailable(f"MiMo API timeout / OS error: {e}") from e

    try:
        content = payload["choices"][0]["message"]["content"]
    except (KeyError, IndexError) as e:
        raise MimoUnavailable(f"MiMo API response missing choices/message/content: {payload}") from e

    try:
        desc = json.loads(content)
    except json.JSONDecodeError as e:
        raise MimoUnavailable(f"MiMo API returned non-JSON content: {content[:400]}") from e

    if not isinstance(desc, dict):
        raise MimoUnavailable(f"MiMo API content is not an object: {desc!r}")
    return desc
