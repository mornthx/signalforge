# cmake/install.cmake
#
# M13 S1: install() rules for V1.0 release packaging.
# Targets layout (per spec §2.1-3):
#
#   /opt/signalforge/bin/{signalforge, sfreplay_inspect, profile_main}
#   /opt/signalforge/docs/                 — V1.0 user docs
#   /opt/signalforge/benchmarks/results/   — M5-M12 baseline.md files
#   /opt/signalforge/tools/profile/        — profile harness scripts
#   /usr/share/applications/signalforge.desktop
#   /usr/share/icons/hicolor/256x256/apps/signalforge.png
#   /usr/share/pixmaps/signalforge.png      — legacy icon path
#
# Symlinks (/usr/local/bin/{signalforge, sfreplay_inspect}) are
# created by the postinst script (cmake/deb-scripts/postinst), not
# by CMake — this avoids shipping broken symlinks in the package
# tree.

# Binaries → /opt/signalforge/bin/
install(TARGETS signalforge
    RUNTIME DESTINATION bin
    COMPONENT Runtime
)
install(TARGETS sfreplay_inspect
    RUNTIME DESTINATION bin
    COMPONENT Runtime
)
install(TARGETS profile_main
    RUNTIME DESTINATION bin
    COMPONENT Runtime
)

# V0.x user docs (copies of the source-tree files at install time).
# Note: V1.0 release notes deferred per V0 series charter
# (`docs/V0-series-charter.md`); V0.1 status summary supersedes them.
# `docs/v1.0-spec-list.md` retained (with disclaimer at top) as the
# authoritative V0.x freeze record.
install(FILES
    docs/V0-series-charter.md
    docs/v0.1-status-summary.md
    docs/install.md
    docs/v1.0-spec-list.md
    docs/m13-hardware-verification.md
    docs/m9-hardware-verification.md
    docs/m10-hardware-verification.md
    docs/m11-hardware-verification.md
    DESTINATION docs
    COMPONENT Documentation
)

# Format spec (frozen at M10 close).
install(FILES
    docs/format/sfreplay-v1.md
    DESTINATION docs/format
    COMPONENT Documentation
)

# Architecture decision records.
install(DIRECTORY docs/architecture/decisions/
    DESTINATION docs/architecture/decisions
    COMPONENT Documentation
    FILES_MATCHING PATTERN "ADR-*.md"
)

# M5-M12 benchmark baselines (V1.0 performance reference).
install(FILES
    tests/benchmark/results/M3-baseline.md
    tests/benchmark/results/M4-baseline.md
    tests/benchmark/results/M5-baseline.md
    tests/benchmark/results/M6-baseline.md
    tests/benchmark/results/M7-baseline.md
    tests/benchmark/results/M8-baseline.md
    tests/benchmark/results/M10-baseline.md
    tests/benchmark/results/M11-baseline.md
    tests/benchmark/results/M12-baseline.md
    tests/benchmark/results/M12-profile-report.md
    tests/benchmark/results/M12-regression-suite.md
    DESTINATION benchmarks/results
    COMPONENT Documentation
)

# Profile harness + regression suite scripts.
install(PROGRAMS
    tools/profile/run_profile.sh
    tools/profile/check_regression.py
    tests/benchmark/m12_regression_suite.sh
    DESTINATION tools/profile
    COMPONENT Runtime
)

# Desktop launcher entry — installed at the absolute system path.
install(FILES installer/signalforge.desktop
    DESTINATION /usr/share/applications
    COMPONENT Runtime
)

# App icons.
install(FILES installer/signalforge.png
    DESTINATION /usr/share/icons/hicolor/256x256/apps
    COMPONENT Runtime
)
install(FILES installer/signalforge.png
    DESTINATION /usr/share/pixmaps
    COMPONENT Runtime
)

# M16 S4: bundled fonts (advisory filesystem copy per
# M16-concerns §C6 hybrid). SignalForge always uses the
# resource-compiled fonts (in-binary; lockstep). The
# filesystem copy at /usr/share/fonts/signalforge/ exists
# for documentation + system-wide font availability for
# tools OUTSIDE SignalForge that want consistent rendering.
# postinst does NOT register these in fontconfig — they're
# advisory, not system fonts.
install(FILES
    resources/fonts/Inter-Regular.otf
    resources/fonts/Inter-Medium.otf
    resources/fonts/Inter-Bold.otf
    resources/fonts/Inter-Italic.otf
    resources/fonts/JetBrainsMono-Regular.ttf
    resources/fonts/JetBrainsMono-Medium.ttf
    DESTINATION /usr/share/fonts/signalforge
    COMPONENT Runtime
)
install(FILES
    resources/fonts/LICENSE-Inter.txt
    resources/fonts/LICENSE-JetBrainsMono.txt
    DESTINATION /usr/share/doc/signalforge/fonts
    COMPONENT Documentation
)
