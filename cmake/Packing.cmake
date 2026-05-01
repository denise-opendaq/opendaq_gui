#
# Packaging configuration for the top-level project via CPack.
#
# This file is included from the root `CMakeLists.txt` and also used as
# `CPACK_PROJECT_CONFIG_FILE` to ensure our CPack settings win over any
# third-party dependencies.
#

include("${CMAKE_CURRENT_LIST_DIR}/version.cmake")

# Non-CPack packaging targets (Windows NSIS, Linux DEB)
# Keep all packaging wiring in this file.
#
# Note: This file is also used as `CPACK_PROJECT_CONFIG_FILE`. CPack can
# evaluate it in contexts where the main target isn't present. Guard all
# packaging targets accordingly.

if(APPLE)
  # productbuild PKG — installs the self-contained app bundle to /Applications.
  # openDAQ runtime is bundled inside the .app by macdeployqt, so we only
  # ship the `app` component. /Applications is on the data volume and is not
  # blocked by SIP, so the old "system volume" error no longer applies.
  set(CPACK_GENERATOR "productbuild")

  set(CPACK_BINARY_TGZ OFF)
  set(CPACK_BINARY_ZIP OFF)

  set(CPACK_COMPONENTS_ALL app)
  set(CPACK_COMPONENTS_GROUPING ALL_COMPONENTS_IN_ONE)
  set(CPACK_PRODUCTBUILD_IDENTIFIER "com.opendaq.qt-gui")

  # Output filename: OpenDAQ.Qt.GUI-<ver>-mac-<arch>.pkg
  if(CMAKE_OSX_ARCHITECTURES)
    set(_PKG_ARCH "${CMAKE_OSX_ARCHITECTURES}")
  else()
    execute_process(COMMAND uname -m OUTPUT_VARIABLE _PKG_ARCH OUTPUT_STRIP_TRAILING_WHITESPACE)
  endif()
  if(_PKG_ARCH MATCHES "arm64|aarch64")
    set(_PKG_ARCH "arm64")
  elseif(_PKG_ARCH MATCHES "x86_64|amd64")
    set(_PKG_ARCH "x86_64")
  endif()

  set(CPACK_PACKAGE_NAME "OpenDAQ.Qt.GUI")
  set(CPACK_PACKAGE_VENDOR "openDAQ")
  set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
  set(CPACK_PACKAGE_FILE_NAME "OpenDAQ.Qt.GUI-${PROJECT_VERSION}-mac-${_PKG_ARCH}")

  set(CPACK_PACKAGING_INSTALL_PREFIX "/")

elseif (UNIX)
  set(CPACK_GENERATOR "DEB")
  set(CPACK_BINARY_TGZ OFF)
  set(CPACK_BINARY_ZIP OFF)

  execute_process(COMMAND uname -m OUTPUT_VARIABLE _PKG_ARCH OUTPUT_STRIP_TRAILING_WHITESPACE)
  if(_PKG_ARCH MATCHES "x86_64|amd64")
    set(_PKG_ARCH "amd64")
  elseif(_PKG_ARCH MATCHES "aarch64|arm64")
    set(_PKG_ARCH "arm64")
  endif()

  set(CPACK_PACKAGE_NAME "OpenDAQ.Qt.GUI")
  set(CPACK_PACKAGE_VENDOR "openDAQ")
  set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
  set(CPACK_PACKAGE_FILE_NAME "OpenDAQ.Qt.GUI-${PROJECT_VERSION}-linux-${_PKG_ARCH}")

  set(CPACK_COMPONENTS_ALL app)
  set(CPACK_COMPONENTS_GROUPING ALL_COMPONENTS_IN_ONE)
  set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")

  set(CPACK_DEBIAN_PACKAGE_MAINTAINER "OpenDAQ <denis.erokhin@opendaq.com>")
  set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6, libstdc++6, libgcc-s1, libx11-6, libxcb1, libgl1")

elseif(WIN32)

  find_program(NSIS_EXECUTABLE makensis PATHS
    "C:/Program Files (x86)/NSIS"
    "C:/Program Files/NSIS"
    ENV NSIS_PATH
  )

  if(NSIS_EXECUTABLE)
    set(CPACK_GENERATOR "NSIS")
    set(CPACK_BINARY_TGZ OFF)
    set(CPACK_BINARY_ZIP OFF)

    set(_PKG_ARCH "x64")
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "ARM64|aarch64")
      set(_PKG_ARCH "arm64")
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "ARM")
      set(_PKG_ARCH "arm")
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "i[3-6]86|X86")
      set(_PKG_ARCH "x86")
    endif()

    set(CPACK_PACKAGE_NAME "OpenDAQ.Qt.GUI")
    set(CPACK_PACKAGE_VENDOR "openDAQ")
    set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
    set(CPACK_PACKAGE_FILE_NAME "OpenDAQ.Qt.GUI-${PROJECT_VERSION}-windows-${_PKG_ARCH}")

    set(CPACK_COMPONENTS_ALL app)
    set(CPACK_COMPONENTS_GROUPING ALL_COMPONENTS_IN_ONE)
    set(CPACK_PACKAGING_INSTALL_PREFIX "/")

    set(CPACK_NSIS_DISPLAY_NAME "OpenDAQ Qt GUI")
    set(CPACK_NSIS_PACKAGE_NAME "OpenDAQ Qt GUI")
    set(CPACK_NSIS_MODIFY_PATH OFF)
  else()
    message(STATUS "NSIS (makensis) not found. Skipping CPack NSIS configuration.")
  endif()
endif()

