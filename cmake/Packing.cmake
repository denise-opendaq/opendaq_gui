#
# Packaging configuration for the top-level project via CPack.
#
# This file is included from the root `CMakeLists.txt` and also used as
# `CPACK_PROJECT_CONFIG_FILE` to ensure our CPack settings win over any
# third-party dependencies.
#

include("${CMAKE_CURRENT_LIST_DIR}/version.cmake")

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
endif()

