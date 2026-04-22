# SignalForge

SignalForge is a Qt 6.10.2 / C++20 desktop workbench for embedded-device
bring-up on Ubuntu 24.04 LTS (x64). Architecture: see
`docs/architecture/architecture.md`. Project rules: see `CLAUDE.md`.

## Build

```
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

`release` and `debug-asan` presets are also available. Qt 6.10.2 must
be installed under `~/Qt/6.10.2/gcc_64/` or at a path exported via the
`SIGNALFORGE_QT_PATH` environment variable.
