# SignalForge — Install Guide (V1.0)

This document covers installing SignalForge V1.0 on Ubuntu
24.04 (the supported V1.0 platform). Three install paths are
covered:

1. [.deb package](#1-deb-package-recommended) — the
   recommended path
2. [Build from source](#2-build-from-source) — for non-Debian
   distros or custom builds
3. [.tar.gz fallback](#3-targz-fallback) — _not produced for
   V1.0 (V1.5+ may add)_

---

## 1. .deb package (recommended)

### 1.1 Prerequisites

- Ubuntu 24.04 LTS (or compatible Debian-based distribution)
- amd64 architecture (`uname -m` returns `x86_64`)
- **Qt 6.10** (newer than Ubuntu 24.04 stock Qt 6.4).

#### About the Qt 6.10 requirement

V1.0 was built and tested against Qt 6.10. The `.deb`'s
`Depends:` field declares the Ubuntu 24.04 stock Qt 6.4
runtime packages (e.g., `libqt6core6t64`, `libqt6quickwidgets6`)
so that `dpkg` is satisfied at install time. **However, the
SignalForge binary actually links against the Qt 6.10 ABI
and will not run against Qt 6.4 alone.** You must install
Qt 6.10 separately. Three options:

  - **Option A (recommended)**: Qt's official installer
    from <https://www.qt.io/download-qt-installer>.
    Default install path is `~/Qt/6.10.x/gcc_64`. After
    install, ensure `libQt6Core.so.6` etc. resolve at
    runtime via either:
    - `export LD_LIBRARY_PATH=$HOME/Qt/6.10.2/gcc_64/lib:$LD_LIBRARY_PATH`
      (in `~/.profile` or your shell rc)
    - or system-wide:
      ```bash
      echo "$HOME/Qt/6.10.2/gcc_64/lib" | sudo tee /etc/ld.so.conf.d/qt6.10.conf
      sudo ldconfig
      ```
  - **Option B**: build Qt 6.10 from source (advanced).
  - **Option C**: a community PPA that ships Qt 6.10
    runtime packages (search apt sources matching your
    Ubuntu point release; not officially endorsed).

Stock Ubuntu 24.04's Qt 6.4 will be installed as a side
effect of `dpkg -i` (it satisfies the declared deps), but
SignalForge will only run against your separately-installed
Qt 6.10. Both Qt 6.4 and Qt 6.10 can coexist.

### 1.2 Install

```bash
# Download signalforge_1.0.0_amd64.deb from
# https://github.com/mornthx/signalforge/releases/tag/v1.0.0

sudo dpkg -i signalforge_1.0.0_amd64.deb

# If dpkg complains about missing dependencies:
sudo apt-get install -f
```

The .deb installs to `/opt/signalforge/` and creates symlinks
at `/usr/local/bin/{signalforge,sfreplay_inspect}` via the
postinst script.

### 1.3 Launch

```bash
# Command line:
signalforge

# Or use the application menu — "SignalForge" entry under
# Development / Engineering / Science.
```

### 1.4 Serial-port access

If you'll connect to USB-serial devices (`/dev/ttyUSB*`,
`/dev/ttyACM*`), add yourself to the `dialout` group:

```bash
sudo usermod -aG dialout $USER
# Log out and back in (or reboot) for the change to apply.
```

### 1.5 Uninstall

```bash
sudo dpkg -r signalforge
```

Your config in `~/.config/signalforge/` is preserved across
removal. To purge config completely:

```bash
rm -rf ~/.config/signalforge/
```

---

## 2. Build from source

For non-Ubuntu distros or custom builds.

### 2.1 Prerequisites

- C++23 compiler (GCC 13+ or Clang 17+)
- CMake 3.22+
- Qt 6.10
- yaml-cpp 0.7+
- Ninja (recommended) or Make

### 2.2 Clone

```bash
git clone https://github.com/mornthx/signalforge.git
cd signalforge
git checkout v1.0.0
```

### 2.3 Configure + build

```bash
# Set Qt 6.10 install path if it's not in standard location:
export SIGNALFORGE_QT_PATH=$HOME/Qt/6.10.2/gcc_64

cmake -B build/release -DCMAKE_BUILD_TYPE=Release
cmake --build build/release -j
```

### 2.4 Run

```bash
build/release/src/app/signalforge
```

### 2.5 Build the .deb (optional)

```bash
cmake --build build/release --target package
# Produces build/release/signalforge_1.0.0_amd64.deb
```

---

## 3. .tar.gz fallback

**Not provided for V1.0.** The V1.0 distribution is .deb only.
For non-Debian distros, build from source ([§2](#2-build-from-source)).

V1.5+ may add a `.tar.gz` artefact for platform-agnostic
install.

---

## Troubleshooting

### `signalforge: command not found`

The postinst script creates symlinks at
`/usr/local/bin/`. Check it ran:

```bash
ls -l /usr/local/bin/signalforge
```

If missing, re-run the postinst manually:

```bash
sudo /var/lib/dpkg/info/signalforge.postinst configure
```

Or manually create the symlink:

```bash
sudo ln -sf /opt/signalforge/bin/signalforge /usr/local/bin/signalforge
```

### `error while loading shared libraries: libQt6Core.so.6`

Qt 6.10 isn't on the library search path. Verify Qt 6.10 is
installed and add to `LD_LIBRARY_PATH`:

```bash
export LD_LIBRARY_PATH=$HOME/Qt/6.10.2/gcc_64/lib:$LD_LIBRARY_PATH
signalforge
```

To make this permanent, add the path to `/etc/ld.so.conf.d/`:

```bash
echo "$HOME/Qt/6.10.2/gcc_64/lib" | sudo tee /etc/ld.so.conf.d/qt6.10.conf
sudo ldconfig
```

### `Permission denied` on `/dev/ttyUSB0`

You're not in the `dialout` group. See [§1.4](#14-serial-port-access).

Alternatively, for one-time access:

```bash
sudo chmod a+rw /dev/ttyUSB0
# (resets at reboot)
```

### `.sfreplay` files don't open by default

Ensure the MIME database picked up the install. Re-run:

```bash
sudo update-mime-database /usr/share/mime/
sudo update-desktop-database /usr/share/applications/
```

### Application crash on startup

Check the crash logs at:

```bash
~/.local/state/signalforge/logs/
```

Or run with verbose logging:

```bash
SF_LOG_LEVEL=debug signalforge 2>&1 | tee /tmp/signalforge.log
```

Report issues at
<https://github.com/mornthx/signalforge/issues> with the log.

### App launches but no signals appear when connected

1. Verify your schema matches the device's frame format.
   Use `tools/schema_lint` to validate the YAML.
2. Check the connection state in the connection panel —
   if it's not `Connected`, see the error message.
3. Open `View → Statistics` (if implemented) to see frames
   in / decode error count.

---

## Where files go

After install:

| Path | Contents |
|---|---|
| `/opt/signalforge/bin/` | binaries (`signalforge`, `sfreplay_inspect`, `profile_main`) |
| `/opt/signalforge/docs/` | user docs (this file, release notes, spec list, hardware verification protocols) |
| `/opt/signalforge/docs/format/sfreplay-v1.md` | binary format spec |
| `/opt/signalforge/docs/architecture/decisions/` | ADR-001 through ADR-007 |
| `/opt/signalforge/benchmarks/results/` | M3-M12 baseline.md performance references |
| `/opt/signalforge/tools/profile/` | profile + regression-suite scripts |
| `/usr/local/bin/{signalforge,sfreplay_inspect}` | command-line symlinks |
| `/usr/share/applications/signalforge.desktop` | application menu entry |
| `/usr/share/icons/hicolor/256x256/apps/signalforge.png` | icon |
| `~/.config/signalforge/` | user config (preserved on removal) |
| `~/.local/state/signalforge/logs/` | runtime logs |

---

## License

MIT. See `LICENSE` in the source tree or
`/opt/signalforge/LICENSE` after install.
