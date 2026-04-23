# M1 Spike — QQuickWidget in QDockWidget

Evidence-gathering program for the M1 milestone. Embeds three
`QQuickWidget` instances in three `QDockWidget` containers and
exercises five integration checks (see
`docs/milestones/M1-qtquick-integration-spike.md`). The final
deliverable of M1 is the spike report at
`docs/spikes/M1-qtquick-integration.md` — not this binary.

## Build

```
cd tools/spike/qquick_dock_test
cmake -B build -S . -G Ninja \
  -DCMAKE_PREFIX_PATH=$HOME/Qt/6.10.2/gcc_64 \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

The spike is isolated — **not** wired into the top-level
`CMakeLists.txt` (per M1 spec §2.1 item 1) and does not link
anything from `src/`.

## Run

Interactive (manual exploration):

```
./build/qquick_dock_test
```

Headless / automated:

```
xvfb-run --auto-servernum ./build/qquick_dock_test --auto-check 1
xvfb-run --auto-servernum ./build/qquick_dock_test --auto-check 3
xvfb-run --auto-servernum ./build/qquick_dock_test --auto-check 4 --short
```

Exit status: 0 on pass, non-zero on fail / unimplemented / unknown
check id.

## Logging exception

This spike uses `qDebug` for logging as an exception to
`CLAUDE.md §Forbidden-6`. M1 spec §S2 and §9 explicitly authorize
the exception inside `tools/spike/**`. Production code under
`src/` continues to use the `SF_LOG_*` macros defined in
`src/observability/logging.hpp`.
