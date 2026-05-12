#!/usr/bin/env python3
"""M16 S3 — QSS selector linter (manifesto + spec §3 M16.4 enforcement).

Enforces QSS discipline rules from
`docs/v0.3/visual-identity.md` §2.3 + §5.3 +
`docs/milestones/M16-visual-identity-ownership.md` §3 M16.4:

  Rejected:
    - `*` universal selector
    - Pseudo-state at root (`*::indicator`, `*:hover`, etc.)
    - 3+ deep descendant chains (`A B C D` etc.)

  Allowed:
    - Class selectors:               `.foo`
    - ID / objectName:               `#foo`
    - Dynamic property:              `QPushButton[class="foo"]`
    - Widget class:                  `QPushButton`, `QLineEdit`
    - Widget pseudo-state:           `QPushButton:focus`, `:disabled`
    - Single-step child:             `QFrame > QLabel`
    - Two-step descendant:           `QFrame QLabel`
    - Comma-separated alternatives:  `QPushButton, QToolButton`

Output:
    file:line:column: rule violated; suggested fix.

CMake / pre-commit integration: lint_qss.py exits non-zero if
any QSS file in `resources/styles/` violates a rule. CI run as
a build pre-step.

Stdlib only — no PyParsing / cssutils deps (CLAUDE.md §1). The
parser is hand-rolled and scoped to QSS subset Qt accepts; not
a full CSS parser. Edge cases (multi-line selectors, nested
attribute brackets) handled minimally.

Usage:
    python3 tools/lint_qss.py [<path> ...]      # lint specific paths
    python3 tools/lint_qss.py                   # lint default: resources/styles/*.qss

Exit code: 0 on clean lint; 1 on violations; 2 on usage error.
"""

from __future__ import annotations

import argparse
import re
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Iterator

REPO_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_TARGETS = (REPO_ROOT / "resources" / "styles",)


@dataclass
class Violation:
    path: Path
    line: int
    column: int
    rule: str
    selector: str
    suggestion: str

    def format(self) -> str:
        return (
            f"{self.path}:{self.line}:{self.column}: "
            f"QSS-LINT {self.rule}: '{self.selector}' — {self.suggestion}"
        )


# ---------------------------------------------------------------------------
# Comment + block-content stripping
# ---------------------------------------------------------------------------


_COMMENT_RE = re.compile(r"/\*.*?\*/", re.DOTALL)


def _strip_comments(text: str) -> str:
    """Replace /* ... */ blocks with same-length newlines so line numbers
    survive."""
    def repl(m: re.Match) -> str:
        return "\n" * m.group(0).count("\n")
    return _COMMENT_RE.sub(repl, text)


# ---------------------------------------------------------------------------
# Selector extraction
# ---------------------------------------------------------------------------


def _selector_blocks(text: str) -> Iterator[tuple[int, int, str]]:
    """Yield (line, column, selector_text) for each rule's selector list.

    A QSS rule is `<selector_list> { <decls> }`. We split on `{`/`}`
    pairs and emit the selector_list portion. Line/column refers to the
    start of the selector_list.
    """
    # Track line/col by scanning character-by-character (small files;
    # acceptable performance).
    n = len(text)
    i = 0
    line = 1
    col = 1
    # Selectors live before the first `{`; after `}` a new selector
    # starts. Skip nested braces (QSS doesn't have nested rules but
    # be safe).
    depth = 0
    sel_start = None
    sel_line = sel_col = 0
    while i < n:
        ch = text[i]
        if ch == "{":
            if depth == 0 and sel_start is not None:
                sel = text[sel_start:i]
                if sel.strip():
                    yield sel_line, sel_col, sel
                sel_start = None
            depth += 1
        elif ch == "}":
            depth -= 1
            if depth == 0:
                # Restart selector capture after this rule.
                sel_start = None
        elif depth == 0:
            if sel_start is None and not ch.isspace():
                sel_start = i
                sel_line = line
                sel_col = col
        # Track line/col
        if ch == "\n":
            line += 1
            col = 1
        else:
            col += 1
        i += 1


def _split_selector_list(sel_text: str) -> list[str]:
    """Split a selector list on top-level commas (ignoring commas inside
    [attribute=value], if any)."""
    parts: list[str] = []
    buf: list[str] = []
    depth = 0
    for ch in sel_text:
        if ch == "[":
            depth += 1
            buf.append(ch)
        elif ch == "]":
            depth -= 1
            buf.append(ch)
        elif ch == "," and depth == 0:
            s = "".join(buf).strip()
            if s:
                parts.append(s)
            buf = []
        else:
            buf.append(ch)
    s = "".join(buf).strip()
    if s:
        parts.append(s)
    return parts


# ---------------------------------------------------------------------------
# Per-selector validators
# ---------------------------------------------------------------------------


def _lint_selector(sel: str, path: Path, line: int, column: int) -> Iterator[Violation]:
    """Apply each rule to one individual selector (already comma-split)."""
    stripped = sel.strip()

    # Rule 1: reject `*` universal selector — anywhere in the selector.
    # E.g. `*`, `*:hover`, `* > QLabel`, `QFrame *` all fail.
    # Allowed: attributes like `[class="*"]` (which is a string value,
    # not a universal selector).
    no_attr = re.sub(r"\[[^\]]*\]", "", stripped)
    if re.search(r"(^|\s)\*(\s|$|:|\>|\+|\~)", no_attr):
        yield Violation(
            path=path, line=line, column=column,
            rule="universal-selector-forbidden",
            selector=stripped,
            suggestion=(
                "Replace `*` with a specific widget class "
                "(QPushButton / QLineEdit / etc.), an objectName (#myId), "
                "or a class property ([class=\"foo\"])."
            ),
        )

    # Rule 2: reject deep descendant chains (3+ space-separated parts).
    # Direct-child combinators `>` reset the "depth" counter as that's
    # the explicit child relationship; only descendant whitespace counts.
    # Split into "combinator groups": parts separated by `>`/`+`/`~`.
    # Within each group, count whitespace-separated tokens (after
    # stripping attributes [..] which may contain spaces).
    depth_text = re.sub(r"\[[^\]]*\]", "_attr_", stripped)
    groups = re.split(r"\s*[>+~]\s*", depth_text)
    for group in groups:
        tokens = group.strip().split()
        if len(tokens) >= 3:
            yield Violation(
                path=path, line=line, column=column,
                rule="deep-descendant-forbidden",
                selector=stripped,
                suggestion=(
                    "Selector has 3+ descendant tokens. Use objectName "
                    "(#myId), a class property ([class=\"foo\"]), or "
                    "an explicit child combinator (`>`) — direct child "
                    "is allowed; deep descendants are not."
                ),
            )
            break


def lint_text(text: str, path: Path) -> Iterator[Violation]:
    stripped = _strip_comments(text)
    for line, column, sel_block in _selector_blocks(stripped):
        for sel in _split_selector_list(sel_block):
            yield from _lint_selector(sel, path, line, column)


def lint_path(path: Path) -> Iterator[Violation]:
    text = path.read_text()
    yield from lint_text(text, path)


def _gather_targets(args: list[Path]) -> list[Path]:
    if not args:
        args = list(DEFAULT_TARGETS)
    files: list[Path] = []
    for a in args:
        if a.is_dir():
            files.extend(sorted(a.rglob("*.qss")))
        elif a.is_file():
            files.append(a)
    return files


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------


def main() -> int:
    parser = argparse.ArgumentParser(
        description="M16 S3 QSS selector linter (manifesto §5.3 / spec §3 M16.4)."
    )
    parser.add_argument("paths", nargs="*", type=Path,
                        help="Files or directories to lint. Default: resources/styles/")
    args = parser.parse_args()

    targets = _gather_targets(args.paths)
    if not targets:
        sys.stderr.write("lint_qss: no .qss files found in target paths\n")
        return 2

    violations: list[Violation] = []
    for t in targets:
        violations.extend(lint_path(t))

    for v in violations:
        sys.stderr.write(v.format() + "\n")

    if violations:
        sys.stderr.write(f"\nQSS lint: {len(violations)} violation(s) in {len(targets)} file(s)\n")
        return 1
    print(f"QSS lint: {len(targets)} file(s) clean")
    return 0


if __name__ == "__main__":
    sys.exit(main())
