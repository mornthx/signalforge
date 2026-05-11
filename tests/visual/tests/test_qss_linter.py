"""M16 S3 — QSS selector linter unit tests.

Exercises `tools/lint_qss.py` against synthetic QSS strings.
Validates each rule independently + the generated tokens.qss
passes (smoke check).

Run via stdlib runner like other tests.
"""

from __future__ import annotations

import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent.parent.parent
sys.path.insert(0, str(REPO_ROOT / "tools"))

import lint_qss  # noqa: E402


def _violations(qss: str, path: Path = Path("test.qss")) -> list[lint_qss.Violation]:
    return list(lint_qss.lint_text(qss, path))


# ---------------------------------------------------------------------------
# Allowed patterns — must produce NO violations
# ---------------------------------------------------------------------------


def test_widget_class_selector_allowed():
    v = _violations("QPushButton { color: red; }")
    assert v == [], v


def test_pseudo_state_allowed():
    v = _violations("QPushButton:focus { color: red; } QLineEdit:disabled { color: gray; }")
    assert v == [], v


def test_object_name_allowed():
    v = _violations("QFrame#panelHeader { background: white; }")
    assert v == [], v


def test_class_property_allowed():
    v = _violations('QLabel[class="status-recording"] { color: red; }')
    assert v == [], v


def test_direct_child_allowed():
    v = _violations("QFrame > QLabel { color: blue; }")
    assert v == [], v


def test_two_descendant_allowed():
    """Two-token descendant chain is allowed; only 3+ rejected."""
    v = _violations("QFrame QLabel { color: blue; }")
    assert v == [], v


def test_comma_alternative_allowed():
    v = _violations("QPushButton, QToolButton { color: red; }")
    assert v == [], v


def test_comments_dont_affect_lint():
    qss = "/* comment with * and weird chars */ QPushButton { color: red; }"
    v = _violations(qss)
    assert v == [], v


def test_pseudo_meta_state_allowed():
    """`:hover`, `:pressed`, etc., on a real class selector are fine."""
    v = _violations("QPushButton:hover { color: red; } QPushButton:pressed { color: blue; }")
    assert v == [], v


# ---------------------------------------------------------------------------
# Universal-selector rule
# ---------------------------------------------------------------------------


def test_universal_selector_alone_rejected():
    v = _violations("* { color: red; }")
    assert len(v) == 1
    assert v[0].rule == "universal-selector-forbidden"


def test_universal_selector_with_pseudo_rejected():
    v = _violations("*:hover { color: red; }")
    assert any(x.rule == "universal-selector-forbidden" for x in v)


def test_universal_selector_in_descendant_rejected():
    v = _violations("QFrame * { color: red; }")
    assert any(x.rule == "universal-selector-forbidden" for x in v)


# ---------------------------------------------------------------------------
# Deep-descendant rule
# ---------------------------------------------------------------------------


def test_three_descendant_rejected():
    v = _violations("QFrame QLabel QLabel { color: red; }")
    assert any(x.rule == "deep-descendant-forbidden" for x in v)


def test_four_descendant_rejected():
    v = _violations("QMainWindow QFrame QLabel QLineEdit { color: red; }")
    assert any(x.rule == "deep-descendant-forbidden" for x in v)


def test_direct_child_does_not_count_as_descendant_depth():
    """`A > B > C` is OK because each child relationship is explicit; not deep descendant."""
    v = _violations("QFrame > QFrame > QLabel { color: blue; }")
    assert v == [], v


def test_attribute_brackets_do_not_count_as_descendant():
    """`[class="..."]` inside a selector doesn't count as a descendant token."""
    v = _violations('QLabel[class="status"][name="x"] { color: red; }')
    assert v == [], v


# ---------------------------------------------------------------------------
# Multi-rule files
# ---------------------------------------------------------------------------


def test_multiple_rules_independent():
    qss = """
    QPushButton { color: red; }
    * { background: gray; }
    QFrame QLabel QLabel { color: blue; }
    """
    v = _violations(qss)
    rules = sorted(x.rule for x in v)
    assert rules == ["deep-descendant-forbidden", "universal-selector-forbidden"], rules


# ---------------------------------------------------------------------------
# Generated tokens.qss smoke
# ---------------------------------------------------------------------------


def test_generated_tokens_qss_passes_linter():
    """The S2-generated tokens.qss must pass the S3 linter (manifesto + spec discipline)."""
    qss_path = REPO_ROOT / "resources" / "styles" / "tokens.qss"
    assert qss_path.is_file(), f"S2 deliverable missing: {qss_path}"
    v = list(lint_qss.lint_path(qss_path))
    assert v == [], (
        "tokens.qss must satisfy linter (M16 S2/S3 discipline). Violations:\n"
        + "\n".join(x.format() for x in v)
    )


if __name__ == "__main__":
    sys.path.insert(0, str(REPO_ROOT / "tests" / "visual"))
    from lib.runner import run_tests
    run_tests(globals())
