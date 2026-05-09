# M13 — Packaging

| Field | Value |
|---|---|
| Milestone ID | M13 |
| Sprint | 13 |
| Estimated effort | 3-5 person-days |
| Prerequisites | M12 closed (main at v0.0.13-alpha.1 or later); accumulated Phase 2 follow-ups must be addressed during M13 |
| Next milestone | None (V1.0 release) |
| Hard-stop type | **V1.0 release readiness** (.deb installable end-to-end, all 18 hardware verification tests pass, 30-min memory soaks pass) + **Documentation completeness** (release notes, install instructions, V1 spec freeze record) |
| Soft-HALT allowed | **No** |
| Branch | `milestone/M13` |
| **Final tag** | **v1.0.0** |

**Cross-reference notation**:

- `[EM §N]` — Execution Manual, section N
- `[Arch §N]` — Architecture document, section N
- `[MR]` — Milestone Roadmap
- `[CM §X]` — CLAUDE.md, section X
- `[ADR-N]` — Architecture Decision Record N
- `[M<n> §N]` — M<n> spec

---

## 1. Goal

M13 is V1.0 **release engineering**. Unlike all previous milestones (functional + optimization), M13:

1. **Packages** the M0-M12 codebase into a Linux .deb installable artifact
2. **Verifies** V1.0 release prerequisites (18 hardware tests + 30-min soaks accumulated from M9-M11)
3. **Documents** the V1 release for end users
4. **Tags** v1.0.0 as the official V1 release

After M13, V1.0 is **shipped**. Users on Ubuntu 24.04 can:

```bash
sudo dpkg -i signalforge_1.0.0_amd64.deb
sudo apt-get install -f  # Resolve any deps
signalforge  # Launches the app
```

The .deb includes the binary + benchmark tools + profile tools + V1 spec docs.

This milestone freezes:

1. **V1.0 release surface** — the entirety of M0-M12 frozen interfaces become V1.0 contract
2. **Distribution format** (`.deb` schema, file paths, post-install scripts)
3. **V1.0 release notes** (canonical V1 user-visible feature list)

After v1.0.0 tag, V1 is locked. Future development is V1.5+ (additive) or V2 (breaking).

Quality philosophy:
- **Real install, real verification**: dogfood-style, not "looks like it would work"
- **Release prerequisites are blocking**: 30-min soaks + 18 hardware tests must pass
- **Documentation is part of release**: install instructions, troubleshooting guide, V1 spec list

---

## 2. Scope

### 2.1 Must deliver

1. **CMake CPack configuration** at `cmake/cpack-deb.cmake` (or in main CMakeLists.txt):
   - Target: Ubuntu 24.04 amd64
   - Dependency manifest:
     - `qt6-base-dev (>= 6.10)`, `qt6-multimedia-dev`, `qt6-quick-dev`, `qt6-serialport-dev`
     - `libstdc++6 (>= 13)`, `libc6 (>= 2.41)`
     - `libyaml-cpp-dev (>= 0.7)`
   - Maintainer info, version, description, license (MIT)
   - Conflicts/Replaces: none for V1.0

2. **Build artifact**:
   - `.deb` package: `signalforge_1.0.0_amd64.deb`
   - Generated via `cmake --build build/release --target package`
   - Installs to `/opt/signalforge/`
   - Symlinks: `/usr/local/bin/signalforge`, `/usr/local/bin/sfreplay_inspect`

3. **Package contents** (per decision M13.2 Y):
   - `/opt/signalforge/bin/signalforge` (main GUI binary)
   - `/opt/signalforge/bin/sfreplay_inspect` (CLI tool)
   - `/opt/signalforge/bin/profile_main` (M12 profile harness)
   - `/opt/signalforge/lib/*` (Qt + standard libs if statically bundled; or rely on system Qt)
   - `/opt/signalforge/docs/` (V1 spec list, release notes, hardware verification protocols)
   - `/opt/signalforge/benchmarks/results/` (M5-M12 baseline.md files)
   - `/opt/signalforge/tools/profile/` (profile harness + regression suite scripts)
   - `/usr/share/applications/signalforge.desktop` (launcher entry)
   - `/usr/share/icons/hicolor/256x256/apps/signalforge.png` (icon)
   - `/usr/share/pixmaps/signalforge.png` (legacy icon path)

4. **Post-install / pre-removal scripts** at `cmake/deb-scripts/`:
   - `postinst`: register `.sfreplay` file association via `update-mime-database`
   - `prerm`: cleanup runtime state (cached config, but preserve user `~/.config/signalforge/`)
   - `postrm`: optional purge of `~/.config/signalforge/` on `apt purge`

5. **`docs/release-notes/v1.0.0.md`**:
   - V1 feature summary (concise, user-facing)
   - System requirements
   - Install instructions
   - Quick start tutorial (5-step example: install → connect → see signals → record → replay)
   - Known limitations (V1.5+ deferred items, V2 territory)
   - Performance baseline summary
   - Acknowledgements / license

6. **`docs/install.md`**:
   - Detailed install instructions (3 paths: .deb, .tar.gz fallback if not Debian, build from source)
   - Permissions / udev rules for serial port access
   - Troubleshooting guide

7. **`docs/v1.0-spec-list.md`** (canonical V1 spec freeze record):
   - List of all M0-M12 frozen .hpp files with sha256
   - List of frozen file format specs (SFREPLAY v1, schemas)
   - List of all 7 ADRs (ADR-001 through ADR-007)
   - V1 architectural invariants

8. **`docs/m13-hardware-verification.md`**:
   - Combined verification protocol (18 tests across M9 + M10 + M11 protocols)
   - Single dogfood session covering: connect/disconnect Serial+TCP+UDP+Replay, recording lifecycle, playback controls, mode transitions, configurations
   - Acceptance: 16/18 (2 failures acceptable for V1.0 if documented; per spec §5)

9. **30-min memory soak verification**:
   - `bench_session_writer --soak 1800` runs (M10 follow-up)
   - `bench_replay --memory-soak 1800` runs (M11 follow-up)
   - Both pass: < 10% VmRSS growth, no leaks under ASan
   - Results documented in `tests/benchmark/results/M13-final-soak.md`

10. **Release artifact verification** (per decision M13.5 V):
    - `.deb` install on clean Ubuntu 24.04 VM (or container)
    - Launch `signalforge` from terminal: GUI opens
    - Connect to mock serial port (socat): signals flow
    - All UI elements responsive
    - Uninstall via `sudo dpkg -r signalforge`: clean removal
    - Documented in `tests/integration/m13-deb-install-test.md`

11. **Integration tests** at `tests/integration/`:
    - `test_m13_deb_package.cpp` — CI-runnable validation that .deb structure matches spec
    - `test_m13_release_artifacts.cpp` — verifies all required files present + launchable

12. **Unit tests** for any new internal utilities:
    - `cmake/deb-scripts/postinst` test (run script in chroot, verify mime registration)
    - `cmake/deb-scripts/prerm` test (run, verify config preservation)

13. **GitHub Release v1.0.0**:
    - Tag `v1.0.0` on M13 merge commit
    - Release notes copy from `docs/release-notes/v1.0.0.md`
    - Attached artifact: `signalforge_1.0.0_amd64.deb`

14. **`.claude/M13-done.md`** with completion report + V1.0 release verification

### 2.2 Must not do

1. **No modifications to M2-M12 frozen `.hpp`**. M13 is packaging-only.
2. **No multi-platform support** in V1.0 (decision M13.1 B — Linux .deb only). V1.5+ may add macOS / Windows.
3. **No new V1 features**. Functional surface is M11 + M12 optimization. M13 packages.
4. **No code signing / notarization** in V1.0 (V1.5+ if needed).
5. **No app store / repository submission** (Ubuntu PPA, Flathub) for V1.0. GitHub Releases only.
6. **No new top-level dependencies** beyond what M0-M12 already require.
7. **No automated release CI**. V1.0 release is manual (run package target, upload to Release).
8. **No backward compatibility breaks** with anything in M0-M12 (frozen).
9. **No bundling of system Qt libs**. Use system Qt 6.10 dependency.
10. **No customizing per-user app data location** beyond M9's `~/.config/signalforge/`.
11. **No localization / i18n** in V1.0 (V1.5+).

---

## 3. Design Decisions (locked by this spec)

### 3.1 Linux .deb only (decision M13.1 Option B)

V1.0 ships as Ubuntu 24.04 amd64 `.deb` package. No tar.gz, no AppImage, no multi-platform.

**Rationale**:
- arch §3 hardware target is "Linux desktop"
- .deb is Ubuntu-native, simpler than tar.gz for end users
- AppImage adds complexity for marginal benefit at V1.0
- Multi-platform is V1.5+ scope expansion (significant work)

**Implication**: V1.0 user must be on Ubuntu 24.04 (or compatible Debian-based). Source-build path documented for other distributions (decision §3.4).

### 3.2 Comprehensive package contents (decision M13.2 Option Y)

Package includes:
- Main binary (signalforge GUI)
- CLI tools (sfreplay_inspect)
- Profile tools (profile_main from M12)
- Documentation
- Benchmark baselines (M5-M12 baseline.md)

**Rationale**:
- Profile tools are useful for V1.5+ optimization work
- Benchmark baselines let users / V1.5+ devs verify their environment
- Docs in install dir: works offline

V1.5+ may extract docs to `/usr/share/doc/signalforge/` for FHS compliance; V1.0 keeps everything in `/opt/signalforge/` for simplicity.

### 3.3 v1.0.0 traditional versioning (decision M13.3 Option P)

Final tag is **`v1.0.0`**.

Throughout M0-M12, alpha tags `v0.0.0-alpha.1` through `v0.0.13-alpha.1` were used. M13 closes V1 development with the canonical `v1.0.0` tag.

**Rationale**:
- Spec consistently uses "V1.0 release"
- Semantic versioning standard
- Future V1.5+ → `v1.5.0`, V2 → `v2.0.0`

### 3.4 GitHub Releases + /opt install (decision M13.4 Option S)

Distribution: **GitHub Releases**. Users download `.deb` from the GitHub release page; install with `dpkg`.

Install path: `/opt/signalforge/`.

Symlinks:
- `/usr/local/bin/signalforge`
- `/usr/local/bin/sfreplay_inspect`

User config: `~/.config/signalforge/` (M9 frozen path)

**Rationale**:
- GitHub Releases is the canonical artifact host for OSS
- `/opt/` is FHS-compliant for "third-party packages"
- Symlinks in `/usr/local/bin/` make CLI tools accessible
- User config separate from system-installed binary

### 3.5 Release prerequisites blocking (decision M13.5 Option V)

V1.0 cannot release until:

1. **18 hardware verification tests pass** (16/18 acceptable)
2. **30-min M10 memory soak passes** (`bench_session_writer --soak 1800`)
3. **30-min M11 memory soak passes** (`bench_replay --memory-soak 1800`)
4. **DEB install verification passes** on clean Ubuntu 24.04

These are **blocking gates**. CC may not propose V1.0 ship until all 4 pass.

**Rationale**:
- 18 hardware tests + 2 soaks have been deferred from M9-M12. V1.0 is the place to close them.
- Real install verification catches packaging bugs missed in unit tests
- Conservative: V1.0 must work end-to-end before tagging

If a prerequisite fails:
- Hardware test fail (16/18 OK; > 2 fail) → HALT, investigate
- Soak fail (memory leak detected) → HALT, file ADR or hotfix
- DEB install fail → HALT, fix CMake / packaging

### 3.6 No soft-HALT (inherits M2-M12)

### 3.7 V1.0 freeze record format

`docs/v1.0-spec-list.md` records:

- Every frozen .hpp from M2-M11 with sha256
- All 7 ADRs (ADR-001 through ADR-007)
- File format specs (SFREPLAY v1)
- M0-M12 spec list
- M13 release artifact paths

This is the **canonical V1 record**. V1.5+ amendments cross-reference this.

---

## 4. Key Implementation Details

### 4.1 CMake CPack configuration

In root `CMakeLists.txt`:

```cmake
# CPack configuration for .deb generation
include(cmake/cpack-deb.cmake)

# In cmake/cpack-deb.cmake:
set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_NAME "signalforge")
set(CPACK_PACKAGE_VERSION "1.0.0")
set(CPACK_PACKAGE_DESCRIPTION "Real-time signal visualization and recording")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "SignalForge - Real-time signal visualization and recording")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "mornthx <user@example.com>")
set(CPACK_DEBIAN_PACKAGE_DEPENDS
    "qt6-base-dev (>= 6.10), libstdc++6 (>= 13), libc6 (>= 2.41), libyaml-cpp-dev (>= 0.7)"
)
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/mornthx/signalforge")
set(CPACK_DEBIAN_PACKAGE_SECTION "science")
set(CPACK_DEBIAN_FILE_NAME "signalforge_1.0.0_amd64.deb")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "/opt/signalforge")

# Post-install / pre-removal scripts
set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
    "${CMAKE_SOURCE_DIR}/cmake/deb-scripts/postinst;${CMAKE_SOURCE_DIR}/cmake/deb-scripts/prerm")

# Strip binary symbols for smaller package
set(CPACK_STRIP_FILES TRUE)

include(CPack)
```

### 4.2 Post-install script

`cmake/deb-scripts/postinst`:

```bash
#!/bin/bash
set -e

# Register .sfreplay MIME type
update-mime-database /usr/share/mime/

# Update icon cache
gtk-update-icon-cache -f -t /usr/share/icons/hicolor/ 2>/dev/null || true

# Update desktop database
update-desktop-database /usr/share/applications/ 2>/dev/null || true

# Set udev rules for /dev/ttyUSB* access (if user is in dialout group)
# (Optional; may require user to add themselves to dialout group)

echo "SignalForge installed to /opt/signalforge/"
echo "Launch via: signalforge or via your application menu"
```

### 4.3 Pre-removal script

`cmake/deb-scripts/prerm`:

```bash
#!/bin/bash
set -e

# Don't remove user config; only reset cached runtime state if any
echo "SignalForge will be removed. User config in ~/.config/signalforge/ will be preserved."
echo "To purge user config, run: rm -rf ~/.config/signalforge/"
```

### 4.4 Desktop entry

`installer/signalforge.desktop`:

```desktop
[Desktop Entry]
Version=1.0
Type=Application
Name=SignalForge
GenericName=Signal Visualization
Comment=Real-time signal visualization and recording
Exec=/usr/local/bin/signalforge %f
Icon=signalforge
Terminal=false
Categories=Development;Engineering;Science;
MimeType=application/x-sfreplay;
```

### 4.5 Hardware verification protocol (combined)

`docs/m13-hardware-verification.md`:

```markdown
# V1.0 Hardware Verification Protocol

Combines M9 (6) + M10 (6) + M11 (6) protocols into a single 18-test dogfood session.

## Setup
- Ubuntu 24.04 host
- Mock connections via socat (Serial), python -m http.server (TCP/UDP)
- Sample SFREPLAY file from M11 fixture

## M9 — Connection Manager (6 tests)
[6 tests from docs/m9-hardware-verification.md]

## M10 — Session Writer (6 tests)
[6 tests from docs/m10-hardware-verification.md]

## M11 — Replay UX (6 tests)
[6 tests from docs/m11-hardware-verification.md]

## Acceptance: 16/18 pass for V1.0 ship
```

### 4.6 Release artifact verification

`tests/integration/m13-deb-install-test.md`:

```markdown
# DEB Install Verification

## Steps
1. Build .deb on dev machine: `cmake --build build/release --target package`
2. Verify package: `dpkg-deb --info signalforge_1.0.0_amd64.deb`
3. Install on clean Ubuntu 24.04: `sudo dpkg -i signalforge_1.0.0_amd64.deb`
4. Launch GUI: `signalforge`
5. Open File → Connections → Add → Serial; configure mock device
6. Verify signals flow into chart
7. Open File → Open Session... → load M11 fixture
8. Verify replay controls work (play, pause, step, seek, speed)
9. Uninstall: `sudo dpkg -r signalforge`
10. Verify clean removal: `dpkg -l signalforge` returns nothing
11. Verify user config preserved: `ls ~/.config/signalforge/`

## Acceptance: All 11 steps pass
```

### 4.7 V1.0 spec list document

`docs/v1.0-spec-list.md`:

```markdown
# V1.0 Spec List

## Frozen .hpp files (M2-M11)

| Module | File | sha256 |
|---|---|---|
| M2 | src/buffer/snapshot.hpp | <hash> |
| M2 | src/decode/decoder_interface.hpp | <hash> |
| M3 | src/drivers/driver_interface.hpp | <hash> |
| ... |

## File format specs

- SFREPLAY v1: docs/format/sfreplay-v1.md
- charts.yaml v1: examples/schemas/charts_v1_schema.json
- connections.yaml v1: schemas/connections_schema_v1.yaml

## Architecture decision records

- ADR-001 (M2 Crashpad → sentry-native pivot)
- ADR-002 (M3 driver topology)
- ADR-003 (M4 frame pipeline)
- ADR-004 (M6 SignalBuffer overhead 30%)
- ADR-005 (M6 SignalBuffer chunked storage)
- ADR-006 (M7 cycle detection in expression engine)
- ADR-007 (M10 SFREPLAY v1 format pivot)

## Milestone specs

- docs/milestones/M0-bootstrap.md through M13-packaging.md

## V1 architectural invariants

- Single-threaded UI
- Lock-free reader path through SignalBuffer
- Bounded staleness ≤ 1ms on chart UI
- Driver workers in background QThread
- Real-time playback timing ±5% (M12 S3 result)
```

---

## 5. Performance gates

### 5.1 Release artifact size

| Metric | Target | HALT |
|---|---|---|
| .deb file size | < 50 MB | > 100 MB |
| Install time on Ubuntu 24.04 (no cache) | < 30s | > 5 min |
| Uninstall time | < 5s | > 30s |

### 5.2 Hardware verification

| Metric | Target | HALT |
|---|---|---|
| 18 tests passing | ≥ 16 | < 12 |

### 5.3 30-min soaks

| Metric | Target | HALT |
|---|---|---|
| M10 SessionWriter VmRSS growth | < 10% | > 15% |
| M11 SessionPlayer VmRSS growth | < 10% | > 15% |

### 5.4 No regression

M0-M12 ctest must remain 587/587 (no new tests, but no failures from packaging changes).

---

## 6. Freeze protocol

### 6.1 What freezes at M13 close

**V1.0 release surface freeze**:
- Entirety of M0-M12 frozen interfaces becomes V1.0 contract (already frozen, M13 just records this)
- Distribution format: `signalforge_1.0.0_amd64.deb` schema
- Release notes content (docs/release-notes/v1.0.0.md)
- Hardware verification protocol (docs/m13-hardware-verification.md)

**V1.0 version invariant**: Once `v1.0.0` is tagged, V1.0 binary contents are immutable. V1.0.1 patch releases would require new tag + new ADR.

### 6.2 What does NOT freeze

- Internal CMake organization (free to refactor)
- Build flags (free to tune)
- Documentation prose (free to clarify in V1.0.x patches)
- CI workflow scripts (free to optimize)

### 6.3 Freeze record format

`docs/v1.0-spec-list.md` is the canonical V1 freeze record (see §4.7).

---

## 7. M13-specific HALT triggers

Beyond CLAUDE.md §HALT:

1. **Modification to M2-M12 frozen `.hpp`** → HALT
2. **DEB build fails** under CMake CPack → HALT, investigate
3. **DEB install fails** on clean Ubuntu 24.04 → HALT, fix packaging
4. **Hardware verification < 16/18 pass** → HALT, investigate failures
5. **30-min soak fails** (memory leak detected) → HALT, file ADR or hotfix
6. **DEB launches but app crashes** → HALT
7. **DEB uninstall leaves files behind** → HALT, fix prerm

---

## 8. Acceptance criteria

### 8.1 Build and test

- [ ] Debug, Release, debug-asan all build clean under C++23
- [ ] All M0-M12 unit + integration tests pass (587/587)
- [ ] CI green on milestone/M13 head

### 8.2 Packaging deliverables

- [ ] `signalforge_1.0.0_amd64.deb` builds successfully via `cmake --build build/release --target package`
- [ ] Package installs cleanly on Ubuntu 24.04
- [ ] All required files present per §2.1-3
- [ ] Post-install + pre-removal scripts work as documented
- [ ] Desktop entry visible in application menu
- [ ] CLI tools accessible via `signalforge` and `sfreplay_inspect` from command line

### 8.3 Release prerequisites (per §5)

- [ ] M10 30-min memory soak passes (< 10% VmRSS growth)
- [ ] M11 30-min memory soak passes (< 10% VmRSS growth)
- [ ] 16/18 hardware verification tests pass
- [ ] DEB install verification all 11 steps pass

### 8.4 Documentation

- [ ] `docs/release-notes/v1.0.0.md` published
- [ ] `docs/install.md` published
- [ ] `docs/v1.0-spec-list.md` published
- [ ] `docs/m13-hardware-verification.md` published

### 8.5 Freeze record

- [ ] M13-done.md has Freezes section
- [ ] V1.0 spec list contains all M2-M11 sha256s
- [ ] All 7 ADRs cross-referenced
- [ ] No modifications to M2-M12 frozen files (sha256 verified)

### 8.6 GitHub Release

- [ ] Tag `v1.0.0` on M13 merge commit
- [ ] GitHub Release v1.0.0 published with release notes
- [ ] `signalforge_1.0.0_amd64.deb` attached as release artifact
- [ ] Release marked as "latest" on GitHub

### 8.7 Hand-off

- [ ] M13-done.md hand-off section covers:
  - V1.0 is shipped
  - V1.5+ optimization roadmap (per M12 profile §6 + S4 finding)
  - V1.5+ feature ideas (multi-file replay, theme, etc.)
  - V2 territory (multi-platform, network sync, encryption)
  - How to file V1.5+ proposal

---

## 9. Notes for CC

- **CMake CPack basics**: read https://cmake.org/cmake/help/latest/cpack_gen/deb.html. The `.deb` build target is `package`. Test locally before assuming it works.

- **Dependencies are the trickiest part**. V1 depends on Qt 6.10 (newer than Ubuntu 24.04 default Qt 6.4). Either:
  - Document this in install.md (user must install Qt 6.10 from external repo)
  - Bundle Qt with the .deb (V1.5+ if needed; complex)
  
  V1.0 documents the requirement; user installs Qt 6.10 themselves.

- **Static linking** of dependencies is V1.5+ work. V1.0 uses dynamic linking with system Qt + standard libs.

- **Release prerequisites are blocking**. Don't skip the hardware verification or soaks to "save time". V1.0 release is a one-shot — get it right.

- **Documentation matters**. Users won't read code; they read release notes and install.md. Make them clear, complete, copy-pasteable.

- **The v1.0.0 tag is final**. Once pushed, we don't retag. Triple-check release notes, install instructions, sha256s before tagging.

- **CMake CPack control extra files**: postinst / prerm must be **executable** (`chmod +x`) or CPack will fail.

- **Don't add new code**. M13 is packaging-only. If you find a code-level issue, file an ADR for V1.0.1 patch instead.

- **Cross-check existing milestone hand-offs**: M12-done.md, M11-done.md, M10-done.md all listed pending follow-ups. M13 closes them.

- **Soak runs may take 30+ min each**. Plan timing accordingly. Soaks are S6 / S7 priority, not first thing.

---

## 10. Closing note

M13 ships V1.0. After v1.0.0 tag:

- **Users can install** SignalForge on Ubuntu 24.04
- **Documentation is complete** for end-users
- **V1 governance** ends; V1.5+ proposals require new milestone or patch tags
- **V1.0 is locked**: no more frozen interface changes (any future change is V1.5+ or V2)

Quality discipline:
- **Real install on real Ubuntu**: verify, don't assume
- **18 hardware tests + 2 soaks all pass**: blocking
- **Release notes are complete**: no "TBD"
- **GitHub Release published**: the artifact is real

After M13:
- V1.5+ work happens on `v1.5-development` branch (or similar)
- V1.0 maintenance via patches: `v1.0.1`, `v1.0.2`, etc.
- V2 development is a complete planning cycle

This is the end of V1 development. Make it count.
