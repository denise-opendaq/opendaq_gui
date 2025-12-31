cmake_minimum_required(VERSION 3.16)

project(opendaq_qt_gui_package)

# Configuration
set(APP_NAME "opendaq-qt-gui")
set(DISPLAY_NAME "OpenDAQ Qt GUI")
set(VERSION "1.0")
set(PROJECT_BINARY_NAME "opendaq_qt_gui")
set(MAINTAINER "OpenDAQ <support@opendaq.io>")
set(DESCRIPTION "OpenDAQ Qt GUI Application")

# Detect architecture
execute_process(COMMAND uname -m OUTPUT_VARIABLE ARCH_RAW OUTPUT_STRIP_TRAILING_WHITESPACE)
if(ARCH_RAW MATCHES "x86_64|amd64")
    set(ARCH "amd64")
elseif(ARCH_RAW MATCHES "aarch64|arm64")
    set(ARCH "arm64")
else()
    set(ARCH "${ARCH_RAW}")
endif()

# Path to the built executable
set(SOURCE_EXECUTABLE "${CMAKE_CURRENT_SOURCE_DIR}/../../build/bin/${PROJECT_BINARY_NAME}")

if(NOT EXISTS "${SOURCE_EXECUTABLE}")
    message(FATAL_ERROR "Executable not found at: ${SOURCE_EXECUTABLE}")
endif()

# Find Qt
find_package(Qt6 REQUIRED COMPONENTS Core)
get_target_property(_qmake_executable Qt6::qmake IMPORTED_LOCATION)
get_filename_component(_qt_bin_dir "${_qmake_executable}" DIRECTORY)

# Get Qt library directory from qmake
execute_process(
    COMMAND "${_qmake_executable}" -query QT_INSTALL_LIBS
    OUTPUT_VARIABLE _qt_lib_dir
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Get Qt plugins directory
execute_process(
    COMMAND "${_qmake_executable}" -query QT_INSTALL_PLUGINS
    OUTPUT_VARIABLE _qt_plugins_dir
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Output paths
set(PACKAGE_DIR "${CMAKE_CURRENT_BINARY_DIR}/package")
set(DEB_ROOT "${PACKAGE_DIR}/${APP_NAME}_${VERSION}")
set(INSTALL_PREFIX "${DEB_ROOT}/usr")
set(DEB_FILE "${CMAKE_CURRENT_BINARY_DIR}/${APP_NAME}_${VERSION}_${ARCH}.deb")

# Create package structure
add_custom_target(prepare_structure ALL
    COMMAND ${CMAKE_COMMAND} -E remove_directory "${PACKAGE_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${INSTALL_PREFIX}/bin"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${INSTALL_PREFIX}/lib/${APP_NAME}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${INSTALL_PREFIX}/share/applications"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${DEB_ROOT}/DEBIAN"
    COMMENT "Creating package structure..."
)

# Copy executable and openDAQ libraries
set(BUILD_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../build/bin")
file(GLOB OPENDAQ_LIBS "${BUILD_LIB_DIR}/*.so*")

add_custom_target(copy_files ALL
    COMMAND ${CMAKE_COMMAND} -E copy "${SOURCE_EXECUTABLE}" "${INSTALL_PREFIX}/bin/${PROJECT_BINARY_NAME}"
    COMMAND chmod +x "${INSTALL_PREFIX}/bin/${PROJECT_BINARY_NAME}"
    DEPENDS prepare_structure
    COMMENT "Copying files..."
)

foreach(lib ${OPENDAQ_LIBS})
    add_custom_command(TARGET copy_files POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${lib}" "${INSTALL_PREFIX}/lib/${APP_NAME}/"
    )
endforeach()

# Bundle Qt libraries and plugins
add_custom_target(bundle_qt ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory "${INSTALL_PREFIX}/lib/${APP_NAME}/qt6/plugins"
    DEPENDS copy_files
    COMMENT "Bundling Qt..."
)

set(QT_LIBS Core Gui Widgets DBus)
foreach(qt_lib ${QT_LIBS})
    if(EXISTS "${_qt_lib_dir}/libQt6${qt_lib}.so.6")
        add_custom_command(TARGET bundle_qt POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_qt_lib_dir}/libQt6${qt_lib}.so.6" "${INSTALL_PREFIX}/lib/${APP_NAME}/qt6/"
        )
    endif()
endforeach()

foreach(plugin_dir platforms platformthemes imageformats iconengines)
    if(EXISTS "${_qt_plugins_dir}/${plugin_dir}")
        add_custom_command(TARGET bundle_qt POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${_qt_plugins_dir}/${plugin_dir}" "${INSTALL_PREFIX}/lib/${APP_NAME}/qt6/plugins/${plugin_dir}"
        )
    endif()
endforeach()

if(EXISTS "${_qt_lib_dir}/libQt6XcbQpa.so.6")
    add_custom_command(TARGET bundle_qt POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_qt_lib_dir}/libQt6XcbQpa.so.6" "${INSTALL_PREFIX}/lib/${APP_NAME}/qt6/"
    )
endif()

file(GLOB ICU_LIBS "${_qt_lib_dir}/libicu*.so.*")
foreach(icu_lib ${ICU_LIBS})
    add_custom_command(TARGET bundle_qt POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${icu_lib}" "${INSTALL_PREFIX}/lib/${APP_NAME}/qt6/"
    )
endforeach()

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/qt.conf" "[Paths]\nPlugins = ../lib/${APP_NAME}/qt6/plugins\n")
add_custom_command(TARGET bundle_qt POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_BINARY_DIR}/qt.conf" "${INSTALL_PREFIX}/bin/qt.conf"
)

# Create desktop file
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/${APP_NAME}.desktop" "\
[Desktop Entry]
Type=Application
Name=${DISPLAY_NAME}
Exec=${PROJECT_BINARY_NAME}
Icon=${APP_NAME}
Categories=Development;Engineering;
Terminal=false
")

add_custom_target(create_files ALL
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_BINARY_DIR}/${APP_NAME}.desktop" "${INSTALL_PREFIX}/share/applications/"
    DEPENDS bundle_qt
)

# Create control file
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/control" "\
Package: ${APP_NAME}
Version: ${VERSION}
Architecture: ${ARCH}
Maintainer: ${MAINTAINER}
Description: ${DESCRIPTION}
 OpenDAQ Qt GUI application with bundled Qt libraries.
Depends: libc6, libstdc++6, libgcc-s1, libx11-6, libxcb1, libgl1
")

add_custom_command(TARGET create_files POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_BINARY_DIR}/control" "${DEB_ROOT}/DEBIAN/control"
)

# Build DEB package
add_custom_target(create_deb
    COMMAND dpkg-deb --build "${DEB_ROOT}" "${DEB_FILE}"
    DEPENDS create_files
    COMMENT "Creating DEB: ${DEB_FILE}"
    BYPRODUCTS "${DEB_FILE}"
)

message(STATUS "==============================================")
message(STATUS "Linux DEB Package Configuration")
message(STATUS "==============================================")
message(STATUS "Architecture:  ${ARCH}")
message(STATUS "Output DEB:    ${DEB_FILE}")
message(STATUS "==============================================")
