"""GENERATED FROM resources/styles/tokens.json — DO NOT EDIT MANUALLY
Generator: tools/generate_style_assets.py
Run `--check` to verify freshness (M16 R15 / H12)

tokens.json version 1.0

Consume design tokens from Python test code. Manifesto principles +
token rationale: see resources/styles/tokens.json `_manifesto_refs`
section.
"""

from __future__ import annotations

from typing import Final

VERSION: Final[str] = "1.0"

# ----- Light theme tokens --------------------------------------------

LIGHT_COLOR: Final[dict[str, str]] = {
    "bg.elevated": "#f5f5f4",
    "bg.primary": "#fbfbfa",
    "bg.surface": "#ffffff",
    "border": "#d6d6d4",
    "border.focus": "#3b7ddd",
    "mode.live": "#2d8a3e",
    "mode.recording": "#c8392a",
    "mode.replay": "#3b7ddd",
    "severity.error": "#c8392a",
    "severity.info": "#5a5d63",
    "severity.warning": "#d4a72c",
    "signal.0": "#2d6cb3",
    "signal.1": "#c8392a",
    "signal.2": "#2d8a3e",
    "signal.3": "#d4a72c",
    "signal.4": "#7e3eb3",
    "signal.5": "#2da3a3",
    "signal.6": "#d4622c",
    "signal.7": "#5a5d63",
    "status.connected": "#2d8a3e",
    "status.connecting": "#d4a72c",
    "status.disconnecting": "#d4a72c",
    "status.error": "#c8392a",
    "status.idle": "#5a5d63",
    "text.disabled": "#9ea1a7",
    "text.primary": "#1a1d23",
    "text.secondary": "#5a5d63",
}

LIGHT_FONT: Final[dict[str, object]] = {
    "family.sans": "Inter",
    "family.mono": "JetBrains Mono",
    "size.display": 18,
    "size.heading": 14,
    "size.body": 12,
    "size.caption": 11,
    "size.mono": 12,
    "weight.regular": 400,
    "weight.medium": 500,
    "weight.bold": 700,
}

LIGHT_SPACING: Final[dict[str, int]] = {
    "xs": 4,
    "sm": 8,
    "md": 16,
    "lg": 24,
    "xl": 32,
}

LIGHT_ICON: Final[dict[str, int]] = {
    "size.sm": 16,
    "size.md": 20,
    "size.lg": 32,
}

