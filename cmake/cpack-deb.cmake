# cmake/cpack-deb.cmake
#
# M13 S1: CPack DEB configuration for V1.0 release per spec §4.1.
# Targets Ubuntu 24.04 amd64. Build via:
#
#   cmake -B build/release -DCMAKE_BUILD_TYPE=Release \
#         -DCMAKE_INSTALL_PREFIX=/opt/signalforge
#   cmake --build build/release --target package
#
# Output: build/release/signalforge_1.0.0_amd64.deb

# CPack DEB uses CPACK_PACKAGING_INSTALL_PREFIX (NOT
# CMAKE_INSTALL_PREFIX) to set the package's installed-file root.
# Spec §3.4: install path is /opt/signalforge/ for relative
# install() DESTINATIONs. Absolute paths (/usr/share/applications,
# /usr/share/icons/...) are honoured regardless.
set(CPACK_PACKAGING_INSTALL_PREFIX "/opt/signalforge")

set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_NAME "signalforge")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION "Real-time signal visualization and recording")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "SignalForge — Real-time signal visualization and recording")
set(CPACK_PACKAGE_VENDOR "mornthx")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/mornthx/signalforge")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_CONTACT "mornthx <noreply@example.com>")

# DEB-specific control fields.
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "mornthx <noreply@example.com>")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/mornthx/signalforge")
set(CPACK_DEBIAN_PACKAGE_SECTION "science")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
set(CPACK_DEBIAN_FILE_NAME "signalforge_${PROJECT_VERSION}_amd64.deb")

# Dependency manifest — Ubuntu 24.04 noble runtime package names.
#
# yaml-cpp is **statically linked** via FetchContent
# (`build/release/_deps/yaml-cpp-build/libyaml-cpp.a`); no runtime
# dep needed. (The original spec §4.1 example listed
# `libyaml-cpp-dev (>= 0.7)` which is build-time and the wrong
# package; M13 corrects this.)
#
# Qt 6 runtime packages are declared at the **stock Ubuntu 24.04
# Qt 6.4** package level — these dependencies satisfy `dpkg`'s
# install gate. Per spec §3.1 + §1.1 of `docs/install.md`, the
# **actual runtime** requires Qt 6.10 (newer than Ubuntu 24.04
# stock); user installs that manually (Qt installer tarball,
# external PPA, etc.). This dual-track is documented in
# `install.md §1.1 Prerequisites`.
#
# `t64` suffix is the Ubuntu 24.04 noble convention for libraries
# rebuilt for the 64-bit `time_t` transition. Some Qt 6 packages
# have it (`libqt6core6t64`, `libqt6gui6t64`, ...) and some don't
# (`libqt6qml6`, `libqt6quick6`, `libqt6quickwidgets6`,
# `libqt6serialport6`).
set(CPACK_DEBIAN_PACKAGE_DEPENDS
    "libqt6quickwidgets6, libqt6quick6, libqt6widgets6t64, \
libqt6core6t64, libqt6qml6, libqt6serialport6, \
libqt6network6t64, libqt6gui6t64, libqt6opengl6t64, libqt6dbus6t64, \
libc6 (>= 2.38), libstdc++6 (>= 13), libgcc-s1, \
libdbus-1-3, libfontconfig1, libfreetype6, libglib2.0-0t64, \
libxcb1, libxkbcommon0, libgl1, libegl1, libglx0, zlib1g"
)

# Strip binary symbols for smaller package size.
set(CPACK_STRIP_FILES TRUE)

# Post-install + pre-removal scripts (must be executable).
set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
    "${CMAKE_SOURCE_DIR}/cmake/deb-scripts/postinst"
    "${CMAKE_SOURCE_DIR}/cmake/deb-scripts/prerm"
)
# CPack 3.22+ auto-`chmod +x`'s the control-extra files when staging.

# Generate `compat` file for older dpkg if needed (Ubuntu 24.04 is
# fine without it; left as a comment for documentation).
# set(CPACK_DEBIAN_PACKAGE_DEBUG ON)  # uncomment for debug output

include(CPack)
