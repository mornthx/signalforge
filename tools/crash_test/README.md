# crash_test — manual verification tool for sentry-native / crashpad

This is a standalone tool, **not** built by the main SignalForge CMake tree.
It deliberately crashes in several ways and relies on sentry-native's
Crashpad backend to produce a minidump in the standard SignalForge crashdump
directory.

## Purpose

Per M2 spec §3.4 and §4.7, deliberate-crash tests are kept out of
`tests/unit/` because AddressSanitizer (in the `debug-asan` preset) would
trip on the crashes and break the test harness. This tool provides the
manual verification path that the spec requires.

## Build

```
cd tools/crash_test
cmake -B build -S .
cmake --build build
```

The build fetches sentry-native 0.7.17 (matching the main project) and its
vendored Chromium Crashpad. The `crashpad_handler` binary is staged next to
`crash_test` by a post-build copy so the tool can spawn it without an
explicit handler path argument.

**Build prerequisite**: `libcurl4-openssl-dev` must be installed on the host
(Crashpad's vendored util/net subdir requires libcurl headers at configure
time, even with transport disabled).

## Run

```
cd tools/crash_test/build
./crash_test null_deref         # dereferences nullptr
./crash_test abort              # calls std::abort()
./crash_test throw              # throws unhandled std::runtime_error
./crash_test stack_overflow     # infinite recursion → SIGSEGV from stack guard
```

Each mode first initializes sentry-native with the standard crashdump
directory, then triggers its crash.

## Verify

After the crash:

1. Check `~/.local/state/signalforge/crashdumps/` for one or more `.dmp`
   files (and possibly `.dmp.metadata.json` siblings). Files are organized
   by run in sentry-native's crashpad database layout:
   ```
   ~/.local/state/signalforge/crashdumps/
     completed/
       <uuid>.dmp
     ...
   ```
2. Verify `.dmp` file size > 0 (an empty file indicates a failed capture).
3. Confirm a `crashpad_handler` process spawned during the crash:
   ```
   ps -ef | grep crashpad_handler     # while the parent is crashing
   journalctl --user _COMM=crashpad_handler --since "5 minutes ago"
   ```

## Troubleshooting

### AppProtection / ld.so.preload interference

Some managed hosts configure `/etc/ld.so.preload` with monitoring or
sanitization runtimes that bind themselves first to the process. This can
interfere with Crashpad's signal handler installation (same class of risk
as M0/M1 surfaced with AddressSanitizer). If the dump directory stays
empty after a confirmed crash:

- Inspect `/etc/ld.so.preload`; if it lists a third-party runtime, try
  running `crash_test` in an environment where that preload is cleared
  (e.g., a clean VM or CI runner).
- Check `dmesg` / journalctl for kernel messages mentioning the handler
  process being killed before it could write the dump.

### crashpad_handler not found

If `sentry_init` logs "handler not found", the post-build copy may have
failed. Verify:

```
ls -la build/crashpad_handler
```

If absent, re-run `cmake --build build` — the custom command fires after
the `crashpad_handler` target itself builds.

## Why not under ASan

Building `crash_test` under the main project's `debug-asan` preset would
have ASan flag each deliberate crash as a violation, terminate early, and
never produce a minidump — defeating the verification. This tool is built
stand-alone (its own CMake) under default Release/Debug flags only.
