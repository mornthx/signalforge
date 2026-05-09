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

# CMAKE_INSTALL_PREFIX overrides /opt/signalforge for the relative
# DESTINATIONs in install.cmake. Absolute paths (/usr/share/...) are
# honoured by CPack DEB regardless.
if(NOT CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT AND NOT DEFINED ENV{DESTDIR})
    # Respect what the user set explicitly.
else()
    set(CMAKE_INSTALL_PREFIX "/opt/signalforge" CACHE PATH "" FORCE)
endif()

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

# Dependency manifest. Per spec §2.1-1, V1.0 targets Qt 6.10 (newer
# than Ubuntu 24.04 default Qt 6.4) — install.md documents the
# requirement that the user installs Qt 6.10 from an external repo.
# We list the package names users will see on Ubuntu, with version
# constraints loose enough for the current (and near-future) Ubuntu
# releases.
set(CPACK_DEBIAN_PACKAGE_DEPENDS
    "libc6 (>= 2.38), libstdc++6 (>= 13), libyaml-cpp0.8 (>= 0.7)"
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
