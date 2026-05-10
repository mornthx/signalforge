# M15 — Visual test suite (V0.2 AI vision infrastructure)

This directory hosts SignalForge's visual regression tests. Each
test launches the production `signalforge` binary headlessly,
captures a screenshot of a known GUI state, and compares it
against a committed baseline at the pixel level.

Public-repo security per `M15-concerns.md` C7: **no vision-LLM
runs in CI**. The CI gate is pixel-level baseline diff only;
vision-LLM verdicts are local-only via CC's multimodal Read
tool or the operator's optional MiMo benchmark.

## Layout

```
tests/visual/
├── README.md                    # this file
├── CMakeLists.txt               # ctest wiring (label: visual)
├── lib/
│   ├── capture.py               # mechanism C in-process launcher
│   ├── compare.py               # stdlib pixel-diff comparator
│   ├── describe.py              # vision-LLM dispatcher (local-only)
│   ├── describe_via_mimo.py     # optional MiMo API wrapper
│   ├── runner.py                # stdlib-only test runner (no pytest)
│   ├── schema.py                # JSON description schema
│   ├── validate_description.py  # schema validator
│   └── prompts/
│       └── cc_native_describe.md
├── baselines/                   # committed PNGs, one per state
└── tests/
    ├── test_states_empty.py
    ├── test_states_with_connection.py
    └── test_states_chart_visible.py
```

## How tests run

Each `test_*.py` exposes top-level `test_*` callables and a
`__main__` block that defers to `lib/runner.py`. CMake registers
one ctest target per test file via `add_test()` invoking
`python3 <file>` directly — no pytest dependency. Tests are
labelled `visual` so they can be filtered:

```sh
cmake --build --preset debug
ctest --preset debug -L visual --output-on-failure
```

Capture flow (mechanism C, per `M15-concerns.md` C1):

1. Launch `signalforge` under `xvfb-run` with isolated
   `XDG_*` dirs.
2. Pass `--auto-load-test-fixture <path>` + optional
   `--auto-select-signal <id>` to drive state.
3. Pass `--capture-screenshot-after-ms <N>` +
   `--capture-screenshot-path <png>` to schedule an
   in-process `MainWindow::captureScreenshot()` call.
4. `--exit-after-ms` shuts the binary down cleanly.

`lib/capture.py` wraps that orchestration. PNG output lands in
`tests/screenshots/<state>.png` (gitignored; ephemeral).
Baselines live at `tests/visual/baselines/<state>.png`
(committed; reviewed once by the operator at S3).

## Adding a new visual test

1. Pick a state name `NN-short-description` (e.g.
   `12-multi-2-drivers`); use the next free `NN` index.
2. Drop a fixture YAML under
   `tests/integration/gui/fixtures/` if the existing fixtures
   don't cover the state. (Fixture format: see
   `tests/integration/gui/fixtures/m14_smoke.yaml`.)
3. Create `tests/visual/tests/test_<name>.py` modelled on
   `test_states_with_connection.py`:

   ```python
   from lib.capture import capture_signalforge_state
   from lib.compare import compare_baseline

   STATE = "12-multi-2-drivers"

   def _ensure_capture():
       actual = REPO_ROOT / "tests/screenshots" / f"{STATE}.png"
       if not actual.is_file():
           capture_signalforge_state(
               state_name=STATE,
               launch_args=["--auto-load-test-fixture",
                            "tests/integration/gui/fixtures/multi.yaml"],
               capture_after_ms=2500,
               exit_after_ms=3500,
               timeout_s=15,
           )
       return actual

   def test_state_matches_baseline():
       cmp = compare_baseline(_ensure_capture(),
                              BASELINES / f"{STATE}.png",
                              max_diff_percent=5.0)
       assert cmp.matched, cmp.note

   if __name__ == "__main__":
       from lib.runner import run_tests
       run_tests(globals())
   ```

4. Reconfigure + build so CMake picks up the new test:

   ```sh
   cmake --build --preset debug
   ctest --preset debug -L visual --output-on-failure
   ```

5. The first run will produce `tests/screenshots/<state>.png`
   and pass with `note=baseline-absent` (per `compare.py`
   policy — new states do not immediately fail).
6. Review the capture (locally, via CC's Read tool, or via the
   operator's eyes). When the capture looks correct:

   ```sh
   scripts/accept-baseline.sh <state-name>
   ```

   The script copies `tests/screenshots/<state>.png` →
   `tests/visual/baselines/<state>.png` and stages it for
   git commit. Review the diff (`git diff --staged`) before
   committing.

## Updating an existing baseline

When an intended UI change makes a baseline obsolete:

1. Run the visual suite locally; the affected test fails
   with a non-zero `diff_percent`.
2. Inspect both PNGs (the actual is in `tests/screenshots/`,
   the baseline in `tests/visual/baselines/`). Use CC's
   Read tool or any image viewer.
3. Confirm the change is intended.
4. Run `scripts/accept-baseline.sh <state-name>`.
5. Commit with a message describing the visual change.

CI uploads `tests/screenshots/**` as the
`visual-screenshots-<preset>` artifact (14-day retention).
On a CI failure, download the artifact, diff against the
committed baseline locally, then promote with
`accept-baseline.sh` if the change is intended.

## Vision-LLM (local-only)

`lib/describe.py` is a backend dispatcher. It returns `None`
unless the operator opts in:

- `SF_VISUAL_DESCRIBE_BACKEND=mimo` + `MIMO_API_KEY=<key>`
  in the operator's shell env → routes through
  `describe_via_mimo.py` (urllib stdlib HTTP to
  `https://api.xiaomimimo.com/v1/chat/completions`).
- Otherwise → returns `None`. Tests treat `None` as a
  no-op (the optional `_optional_description` test asserts
  schema only when a description is available).

CC's multimodal Read tool is the primary interactive vision
backend; it is not invoked from test code (CC drives it
directly during a session). MiMo is the optional benchmark.

**Never commit an API key.** `.env*` is gitignored. CI
workflows reference no secrets for vision-LLM purposes
(HALT trigger H7 fires on accidental references).

## Cross-references

- M15 spec: `docs/milestones/M15-vision-infrastructure.md`
- Concerns: `.claude/M15-concerns.md`
- V0 charter: `docs/V0-series-charter.md`
- Accept-baseline script: `scripts/accept-baseline.sh`
- Capture method: `MainWindow::captureScreenshot` +
  `--capture-screenshot-after-ms` / `--capture-screenshot-path`
  CLI flags (M15 S1).
