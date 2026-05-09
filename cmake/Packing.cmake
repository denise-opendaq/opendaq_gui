#
# App-specific CPack overrides on top of openDAQ's packaging base.
#
# openDAQ (included as a subdirectory) calls include(CPack) first and sets up
# the generator (productbuild / DEB / NSIS) plus its SDK component list.
# This file overrides the identity/prefix/generator for the Qt GUI app and
# restricts packaging to the "app" component only, then calls include(CPack)
# again to regenerate CPackConfig.cmake with these settings winning.
# Calling include(CPack) twice is safe — add_custom_target(package) is guarded.
#

include("${CMAKE_CURRENT_LIST_DIR}/version.cmake")

# ── Architecture ─────────────────────────────────────────────────────────────

if(WIN32)
    set(_PKG_ARCH "x64")
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "ARM64|aarch64")
        set(_PKG_ARCH "arm64")
    endif()
else()
    if(CMAKE_OSX_ARCHITECTURES)
        set(_PKG_ARCH "${CMAKE_OSX_ARCHITECTURES}")
    else()
        execute_process(COMMAND uname -m OUTPUT_VARIABLE _PKG_ARCH OUTPUT_STRIP_TRAILING_WHITESPACE)
    endif()
    if(_PKG_ARCH MATCHES "arm64|aarch64")
        set(_PKG_ARCH "arm64")
    else()
        set(_PKG_ARCH "x86_64")
    endif()
endif()

# ── Package identity ──────────────────────────────────────────────────────────

set(CPACK_PACKAGE_NAME    "OpenDAQ.Qt.GUI")
set(CPACK_PACKAGE_VENDOR  "openDAQ")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")

# ── Platform overrides ────────────────────────────────────────────────────────

if(APPLE)
    set(CPACK_GENERATOR "productbuild")
    set(CPACK_PACKAGE_FILE_NAME      "OpenDAQ.Qt.GUI-${PROJECT_VERSION}-mac-${_PKG_ARCH}")
    set(CPACK_PRODUCTBUILD_IDENTIFIER "com.opendaq.qt-gui")
    set(CPACK_PACKAGING_INSTALL_PREFIX "/")

elseif(UNIX)
    # openDAQ adds ZIP/TXZ — keep only DEB for the app installer.
    set(CPACK_GENERATOR "DEB")
    set(CPACK_PACKAGE_FILE_NAME "OpenDAQ.Qt.GUI-${PROJECT_VERSION}-linux-${_PKG_ARCH}")
    set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "OpenDAQ <denis.erokhin@opendaq.com>")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS    "libc6, libstdc++6, libgcc-s1, libx11-6, libxcb1, libgl1")

elseif(WIN32)
    # openDAQ adds ZIP/TXZ — keep only NSIS for the app installer.
    set(CPACK_GENERATOR "NSIS")
    set(CPACK_PACKAGE_FILE_NAME "OpenDAQ.Qt.GUI-${PROJECT_VERSION}-windows-${_PKG_ARCH}")
    set(CPACK_NSIS_DISPLAY_NAME "OpenDAQ Qt GUI")
    set(CPACK_NSIS_PACKAGE_NAME "OpenDAQ Qt GUI")
    set(CPACK_NSIS_MODIFY_PATH OFF)
    # CPACK_NSIS_MUI_ICON / INSTALLED_ICON_NAME set in bundle_configuration.cmake
endif()

# ── Component filter ──────────────────────────────────────────────────────────
# Only package our "app" component.  openDAQ SDK components (External, mocks,
# signal_generator, …) are not shipped and may not even be built.

set(CPACK_COMPONENTS_ALL app)
set(CPACK_COMPONENTS_GROUPING ALL_COMPONENTS_IN_ONE)

include(CPack)
