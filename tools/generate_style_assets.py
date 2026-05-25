#!/usr/bin/env python3
"""M16 S2 — design token consumer generator.

Reads `resources/styles/tokens.json` (canonical source per
M16-concerns §C4 + R15). Generates 3 consumer files:

  resources/styles/tokens.qss             (Qt stylesheet snippet)
  src/app/generated_style_tokens.hpp      (C++ constexpr + helpers)
  tests/visual/lib/generated_tokens.py    (Python test consumer)

Modes:

  python3 tools/generate_style_assets.py
      Regenerate all 3 consumer files. Overwrites without prompt.

  python3 tools/generate_style_assets.py --check
      CI / pre-commit gate. Re-generate to temp + diff vs
      committed; exit non-zero if any file would change.
      Per R15 / H12: catches "tokens.json edited but consumers
      stale" failure mode.

  python3 tools/generate_style_assets.py --validate
      Validate tokens.json against tokens.schema.json. Stdlib-
      only (no jsonschema dep); minimal subset of JSON Schema
      2020-12 enforced — required keys, type, pattern, range.

Stdlib only per CLAUDE.md §1 (no jsonschema / pyyaml deps).
"""

from __future__ import annotations

import argparse
import json
import re
import sys
import tempfile
from pathlib import Path
from typing import Any

REPO_ROOT = Path(__file__).resolve().parent.parent
TOKENS_JSON = REPO_ROOT / "resources" / "styles" / "tokens.json"
SCHEMA_JSON = REPO_ROOT / "resources" / "styles" / "tokens.schema.json"

OUTPUT_QSS  = REPO_ROOT / "resources" / "styles" / "tokens.qss"
OUTPUT_HPP  = REPO_ROOT / "src" / "app" / "generated_style_tokens.hpp"
OUTPUT_PY   = REPO_ROOT / "tests" / "visual" / "lib" / "generated_tokens.py"

DO_NOT_EDIT_HEADER = (
    "GENERATED FROM resources/styles/tokens.json — DO NOT EDIT MANUALLY\n"
    "Generator: tools/generate_style_assets.py\n"
    "Run `--check` to verify freshness (M16 R15 / H12)"
)


# ----- token loading ---------------------------------------------------------


def load_tokens() -> dict[str, Any]:
    if not TOKENS_JSON.is_file():
        sys.stderr.write(f"tokens.json missing at {TOKENS_JSON}\n")
        sys.exit(2)
    with TOKENS_JSON.open() as f:
        return json.load(f)


def light_theme(tokens: dict[str, Any]) -> dict[str, Any]:
    themes = tokens.get("themes", {})
    light = themes.get("light")
    if light is None:
        sys.stderr.write("tokens.json: themes.light is required (M16 spec §3 M16.2)\n")
        sys.exit(2)
    return light


# ----- minimal JSON Schema validation (stdlib only) --------------------------


HEX_COLOR_RE = re.compile(r"^#[0-9a-fA-F]{6}([0-9a-fA-F]{2})?$")
SPACING_KEY_RE = re.compile(r"^(xs|sm|md|lg|xl|2xl|3xl)$")
COLOR_KEY_RE = re.compile(r"^[a-z]+(\.[a-z0-9_]+)*$")
VERSION_RE = re.compile(r"^[0-9]+\.[0-9]+$")


def validate_tokens(tokens: dict[str, Any]) -> list[str]:
    """Stdlib-only subset of tokens.schema.json validation.

    Catches the rules our generator depends on. Full JSON Schema
    validation could be added later with the jsonschema dep —
    declined per CLAUDE.md §1 no-new-deps; this subset is
    sufficient for token-correctness gates.
    """
    errors: list[str] = []

    if tokens.get("$schema") != "tokens.schema.json":
        errors.append("'$schema' must be exactly 'tokens.schema.json'")

    version = tokens.get("version", "")
    if not VERSION_RE.match(version):
        errors.append(f"'version' must match MAJOR.MINOR (got {version!r})")

    themes = tokens.get("themes")
    if not isinstance(themes, dict):
        errors.append("'themes' must be an object")
        return errors
    if "light" not in themes:
        errors.append("'themes.light' is required (M16 spec)")
    for theme_name, theme in themes.items():
        if theme_name not in ("light", "dark"):
            errors.append(f"'themes.{theme_name}' unknown (only light + dark allowed)")
            continue
        errors.extend(_validate_theme(theme_name, theme))

    return errors


def _validate_theme(name: str, theme: dict[str, Any]) -> list[str]:
    errors: list[str] = []
    required = ("color", "font", "spacing", "icon")
    for key in required:
        if key not in theme:
            errors.append(f"themes.{name} missing required key: {key}")

    # color: hex pattern + key pattern
    for cname, cval in theme.get("color", {}).items():
        if not COLOR_KEY_RE.match(cname):
            errors.append(f"themes.{name}.color.{cname}: key not lowercase.dotted pattern")
        if not isinstance(cval, str) or not HEX_COLOR_RE.match(cval):
            errors.append(f"themes.{name}.color.{cname}: value must be #RRGGBB or #RRGGBBAA (got {cval!r})")

    # font: required keys + ranges
    font = theme.get("font", {})
    for k in ("family.sans", "family.mono"):
        if not isinstance(font.get(k), str) or not font.get(k):
            errors.append(f"themes.{name}.font.{k}: required non-empty string")
    for k in ("size.display", "size.heading", "size.body", "size.caption", "size.mono"):
        v = font.get(k)
        if v is not None and not (isinstance(v, int) and 8 <= v <= 72):
            errors.append(f"themes.{name}.font.{k}: integer 8..72 (got {v!r})")
    for k in ("weight.regular", "weight.medium", "weight.bold"):
        v = font.get(k)
        if v is not None and not (isinstance(v, int) and 100 <= v <= 900):
            errors.append(f"themes.{name}.font.{k}: integer 100..900 (got {v!r})")
    if not isinstance(font.get("size.body"), int):
        errors.append(f"themes.{name}.font.size.body: required integer")
    if not isinstance(font.get("weight.regular"), int):
        errors.append(f"themes.{name}.font.weight.regular: required integer")

    # spacing: required xs/sm/md/lg/xl; multiples of 4
    spacing = theme.get("spacing", {})
    for required_key in ("xs", "sm", "md", "lg", "xl"):
        if required_key not in spacing:
            errors.append(f"themes.{name}.spacing missing required key: {required_key}")
    for sname, sval in spacing.items():
        if not SPACING_KEY_RE.match(sname):
            errors.append(f"themes.{name}.spacing.{sname}: key must be xs/sm/md/lg/xl/2xl/3xl")
        elif not isinstance(sval, int) or sval < 0 or sval % 4 != 0:
            errors.append(f"themes.{name}.spacing.{sname}: must be non-negative integer multiple of 4 (got {sval!r})")

    # icon: required keys + range
    icon = theme.get("icon", {})
    for k in ("size.sm", "size.md", "size.lg"):
        v = icon.get(k)
        if not (isinstance(v, int) and 8 <= v <= 64):
            errors.append(f"themes.{name}.icon.{k}: integer 8..64 (got {v!r})")
    for k in icon.keys():
        if k not in ("size.sm", "size.md", "size.lg"):
            errors.append(f"themes.{name}.icon.{k}: unknown key (only size.sm/md/lg allowed)")

    return errors


# ----- generators ------------------------------------------------------------


def gen_qss(light: dict[str, Any], version: str) -> str:
    color = light["color"]
    font = light["font"]
    spacing = light["spacing"]
    lines: list[str] = []
    lines.append("/*")
    for line in DO_NOT_EDIT_HEADER.splitlines():
        lines.append(f" * {line}")
    lines.append(f" * tokens.json version {version}")
    lines.append(" *")
    lines.append(" * M16 S2: token snippet scaffold. S4 SignalForgeStyle loads this")
    lines.append(" * via QApplication::setStyleSheet after Fusion + palette init.")
    lines.append(" * QSS discipline (per manifesto + spec §3 M16.4): objectName /")
    lines.append(" * class-name / dynamic-property selectors only; no `*` or deep")
    lines.append(" * descendants; no hot-path setStyleSheet.")
    lines.append(" */")
    lines.append("")
    lines.append("/* ===== Top-level surfaces ===== */")
    lines.append("QMainWindow {")
    lines.append(f"    background-color: {color['bg.primary']};")
    lines.append(f"    color: {color['text.primary']};")
    lines.append("}")
    lines.append("")
    lines.append("QDockWidget {")
    lines.append(f"    background-color: {color['bg.surface']};")
    lines.append(f"    color: {color['text.primary']};")
    lines.append("}")
    lines.append("")
    lines.append("QStatusBar {")
    lines.append(f"    background-color: {color['bg.primary']};")
    lines.append(f"    color: {color['text.secondary']};")
    lines.append(f"    border-top: 1px solid {color['border']};")
    lines.append("}")
    lines.append("")
    lines.append("/* ===== Controls ===== */")
    lines.append("QPushButton {")
    lines.append(f"    background-color: {color['bg.elevated']};")
    lines.append(f"    color: {color['text.primary']};")
    lines.append(f"    border: 1px solid {color['border']};")
    lines.append(f"    padding: {spacing['xs']}px {spacing['sm']}px;")
    lines.append("}")
    lines.append("QPushButton:focus {")
    lines.append(f"    border: 1px solid {color['border.focus']};")
    lines.append("}")
    lines.append("QPushButton:disabled {")
    lines.append(f"    color: {color['text.disabled']};")
    lines.append("}")
    lines.append("")
    lines.append("QLineEdit, QComboBox, QSpinBox, QDoubleSpinBox {")
    lines.append(f"    background-color: {color['bg.surface']};")
    lines.append(f"    color: {color['text.primary']};")
    lines.append(f"    border: 1px solid {color['border']};")
    lines.append(f"    padding: {spacing['xs']}px {spacing['sm']}px;")
    lines.append("}")
    lines.append("QLineEdit:focus, QComboBox:focus, QSpinBox:focus, QDoubleSpinBox:focus {")
    lines.append(f"    border: 1px solid {color['border.focus']};")
    lines.append("}")
    lines.append("")
    lines.append("/* ===== Panel chrome ===== */")
    lines.append("QHeaderView::section, QFrame#panelHeader {")
    lines.append(f"    background-color: {color['bg.elevated']};")
    lines.append(f"    color: {color['text.secondary']};")
    lines.append(f"    border: none;")
    lines.append(f"    border-bottom: 1px solid {color['border']};")
    lines.append(f"    padding: {spacing['xs']}px {spacing['sm']}px;")
    lines.append("}")
    lines.append("")
    lines.append("/* ===== Status-bar semantic classes (consumed by M17 widgets) ===== */")
    lines.append("QLabel[class=\"status-idle\"]         { color: " + color["status.idle"] + "; }")
    lines.append("QLabel[class=\"status-connecting\"]   { color: " + color["status.connecting"] + "; }")
    lines.append("QLabel[class=\"status-connected\"]    { color: " + color["status.connected"] + "; }")
    lines.append("QLabel[class=\"status-disconnecting\"]{ color: " + color["status.disconnecting"] + "; }")
    lines.append("QLabel[class=\"status-error\"]        { color: " + color["status.error"] + "; }")
    lines.append("QLabel[class=\"mode-live\"]           { color: " + color["mode.live"] + "; }")
    lines.append("QLabel[class=\"mode-recording\"]      { color: " + color["mode.recording"] + "; }")
    lines.append("QLabel[class=\"mode-replay\"]         { color: " + color["mode.replay"] + "; }")
    lines.append("QLabel[class=\"mode-paused\"]         { color: " + color["mode.paused"] + "; }")
    lines.append("QLabel[class=\"mode-ended\"]          { color: " + color["mode.ended"] + "; }")
    lines.append("QLabel[class=\"severity-info\"]       { color: " + color["severity.info"] + "; }")
    lines.append("QLabel[class=\"severity-warning\"]    { color: " + color["severity.warning"] + "; }")
    lines.append("QLabel[class=\"severity-error\"]      { color: " + color["severity.error"] + "; }")
    lines.append("")
    lines.append("/* ===== Typography hooks ===== */")
    lines.append("QLabel[class=\"display\"]  { font-size: " + str(font["size.display"]) + "px; font-weight: " + str(font["weight.medium"]) + "; }")
    lines.append("QLabel[class=\"heading\"]  { font-size: " + str(font["size.heading"]) + "px; font-weight: " + str(font["weight.medium"]) + "; }")
    lines.append("QLabel[class=\"caption\"]  { font-size: " + str(font["size.caption"]) + "px; color: " + color["text.secondary"] + "; }")
    lines.append("QLabel[class=\"mono\"]     { font-family: \"" + font["family.mono"] + "\"; font-size: " + str(font["size.mono"]) + "px; }")
    lines.append("")
    return "\n".join(lines) + "\n"


def _cpp_token_name(key: str) -> str:
    """Convert 'bg.primary' / 'status.connecting' / 'signal.0' → CppPascalCase."""
    parts = re.split(r"[._]", key)
    return "".join(p[:1].upper() + p[1:] for p in parts)


def gen_hpp(light: dict[str, Any], version: str) -> str:
    color = light["color"]
    font = light["font"]
    spacing = light["spacing"]
    icon = light["icon"]
    lines: list[str] = []
    lines.append("#pragma once")
    lines.append("")
    lines.append("// " + DO_NOT_EDIT_HEADER.replace("\n", "\n// "))
    lines.append(f"// tokens.json version {version}")
    lines.append("")
    lines.append("// Consume design tokens from C++ widget / styling code. Manifesto")
    lines.append("// principles + token rationale: see resources/styles/tokens.json")
    lines.append("// `_manifesto_refs` section.")
    lines.append("")
    lines.append("#include <QColor>")
    lines.append("#include <QString>")
    lines.append("")
    lines.append("namespace signalforge::tokens::light {")
    lines.append("")
    lines.append("// ----- Color hex strings (string-form for QSS interop) -------------")
    lines.append("")
    for k in sorted(color.keys()):
        cname = _cpp_token_name("color." + k)
        # strip leading "Color" to keep names compact
        if cname.startswith("Color"):
            cname = cname[len("Color"):]
        lines.append(f'inline constexpr const char* k{cname}Hex = "{color[k]}";')
    lines.append("")
    lines.append("// ----- Color QColor accessors (inline; QColor not constexpr) ------")
    lines.append("")
    for k in sorted(color.keys()):
        cname = _cpp_token_name("color." + k)
        if cname.startswith("Color"):
            cname = cname[len("Color"):]
        lines.append(f'inline QColor {cname[:1].lower() + cname[1:]}() {{ return QColor(QString::fromLatin1("{color[k]}")); }}')
    lines.append("")
    lines.append("// ----- Font ---------------------------------------------------------")
    lines.append("")
    lines.append(f'inline constexpr const char* kFontFamilySans = "{font["family.sans"]}";')
    lines.append(f'inline constexpr const char* kFontFamilyMono = "{font["family.mono"]}";')
    lines.append(f'inline constexpr int kFontSizeDisplay = {font["size.display"]};')
    lines.append(f'inline constexpr int kFontSizeHeading = {font["size.heading"]};')
    lines.append(f'inline constexpr int kFontSizeBody    = {font["size.body"]};')
    lines.append(f'inline constexpr int kFontSizeCaption = {font["size.caption"]};')
    lines.append(f'inline constexpr int kFontSizeMono    = {font["size.mono"]};')
    lines.append(f'inline constexpr int kFontWeightRegular = {font["weight.regular"]};')
    lines.append(f'inline constexpr int kFontWeightMedium  = {font["weight.medium"]};')
    lines.append(f'inline constexpr int kFontWeightBold    = {font["weight.bold"]};')
    lines.append("")
    lines.append("// ----- Spacing (px) -------------------------------------------------")
    lines.append("")
    for k in ("xs", "sm", "md", "lg", "xl"):
        if k in spacing:
            lines.append(f'inline constexpr int kSpacing{k[:1].upper()}{k[1:]} = {spacing[k]};')
    lines.append("")
    lines.append("// ----- Icon sizes (px) ----------------------------------------------")
    lines.append("")
    lines.append(f'inline constexpr int kIconSm = {icon["size.sm"]};')
    lines.append(f'inline constexpr int kIconMd = {icon["size.md"]};')
    lines.append(f'inline constexpr int kIconLg = {icon["size.lg"]};')
    lines.append("")
    lines.append("}  // namespace signalforge::tokens::light")
    return "\n".join(lines) + "\n"


def gen_py(light: dict[str, Any], version: str) -> str:
    color = light["color"]
    font = light["font"]
    spacing = light["spacing"]
    icon = light["icon"]
    lines: list[str] = []
    lines.append('"""' + DO_NOT_EDIT_HEADER + '\n')
    lines.append(f"tokens.json version {version}\n")
    lines.append("Consume design tokens from Python test code. Manifesto principles +")
    lines.append("token rationale: see resources/styles/tokens.json `_manifesto_refs`")
    lines.append('section.\n"""')
    lines.append("")
    lines.append("from __future__ import annotations")
    lines.append("")
    lines.append("from typing import Final")
    lines.append("")
    lines.append(f'VERSION: Final[str] = "{version}"')
    lines.append("")
    lines.append("# ----- Light theme tokens --------------------------------------------")
    lines.append("")
    lines.append("LIGHT_COLOR: Final[dict[str, str]] = {")
    for k in sorted(color.keys()):
        lines.append(f'    "{k}": "{color[k]}",')
    lines.append("}")
    lines.append("")
    lines.append("LIGHT_FONT: Final[dict[str, object]] = {")
    for k in ("family.sans", "family.mono"):
        lines.append(f'    "{k}": "{font[k]}",')
    for k in ("size.display", "size.heading", "size.body", "size.caption", "size.mono"):
        if k in font:
            lines.append(f'    "{k}": {font[k]},')
    for k in ("weight.regular", "weight.medium", "weight.bold"):
        if k in font:
            lines.append(f'    "{k}": {font[k]},')
    lines.append("}")
    lines.append("")
    lines.append("LIGHT_SPACING: Final[dict[str, int]] = {")
    for k in ("xs", "sm", "md", "lg", "xl"):
        if k in spacing:
            lines.append(f'    "{k}": {spacing[k]},')
    lines.append("}")
    lines.append("")
    lines.append("LIGHT_ICON: Final[dict[str, int]] = {")
    for k in ("size.sm", "size.md", "size.lg"):
        lines.append(f'    "{k}": {icon[k]},')
    lines.append("}")
    lines.append("")
    return "\n".join(lines) + "\n"


# ----- entry points ----------------------------------------------------------


def write_file(path: Path, content: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content)


def check_mode() -> int:
    """Re-generate to a temp dir + diff vs committed. Exit non-zero on drift."""
    tokens = load_tokens()
    errors = validate_tokens(tokens)
    if errors:
        sys.stderr.write("tokens.json validation failed:\n")
        for e in errors:
            sys.stderr.write(f"  - {e}\n")
        return 2
    light = light_theme(tokens)
    version = tokens["version"]

    expected = {
        OUTPUT_QSS: gen_qss(light, version),
        OUTPUT_HPP: gen_hpp(light, version),
        OUTPUT_PY:  gen_py(light, version),
    }

    drift: list[Path] = []
    for path, expected_content in expected.items():
        if not path.is_file():
            sys.stderr.write(f"--check: {path.relative_to(REPO_ROOT)} missing (regenerate)\n")
            drift.append(path)
            continue
        actual = path.read_text()
        if actual != expected_content:
            sys.stderr.write(f"--check: {path.relative_to(REPO_ROOT)} stale (regenerate + commit)\n")
            drift.append(path)

    if drift:
        sys.stderr.write("\nFix: python3 tools/generate_style_assets.py\n")
        sys.stderr.write("Then: git add resources/styles/tokens.qss src/app/generated_style_tokens.hpp tests/visual/lib/generated_tokens.py\n")
        sys.stderr.write("\nM16 governance: H12 (generated assets drift). See M16-plan §4.\n")
        return 1

    print(f"--check: all generated files up to date with tokens.json v{version}")
    return 0


def generate_mode() -> int:
    tokens = load_tokens()
    errors = validate_tokens(tokens)
    if errors:
        sys.stderr.write("tokens.json validation failed:\n")
        for e in errors:
            sys.stderr.write(f"  - {e}\n")
        return 2
    light = light_theme(tokens)
    version = tokens["version"]

    write_file(OUTPUT_QSS, gen_qss(light, version))
    write_file(OUTPUT_HPP, gen_hpp(light, version))
    write_file(OUTPUT_PY,  gen_py(light, version))

    print(f"Generated tokens v{version}:")
    for p in (OUTPUT_QSS, OUTPUT_HPP, OUTPUT_PY):
        print(f"  {p.relative_to(REPO_ROOT)}  ({p.stat().st_size} bytes)")
    return 0


def validate_mode() -> int:
    tokens = load_tokens()
    errors = validate_tokens(tokens)
    if errors:
        sys.stderr.write("tokens.json validation failed:\n")
        for e in errors:
            sys.stderr.write(f"  - {e}\n")
        return 1
    version = tokens["version"]
    print(f"tokens.json v{version}: validation PASS")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(
        description="M16 S2 design token generator + validator (R15 enforcement)."
    )
    parser.add_argument("--check", action="store_true",
                        help="CI / pre-commit gate: re-generate to temp + diff; non-zero on drift")
    parser.add_argument("--validate", action="store_true",
                        help="Validate tokens.json against schema; exit on errors")
    args = parser.parse_args()

    if args.check and args.validate:
        sys.stderr.write("--check and --validate are mutually exclusive\n")
        return 2

    if args.check:
        return check_mode()
    if args.validate:
        return validate_mode()
    return generate_mode()


if __name__ == "__main__":
    sys.exit(main())
