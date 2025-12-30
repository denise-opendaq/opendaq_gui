cmake_minimum_required(VERSION 3.16)

project(opendaq_qt_gui_package)

# Configuration
set(APP_NAME "OpenDAQ Qt GUI")
set(BUNDLE_IDENTIFIER "com.opendaq.qt-gui")
set(VERSION "1.0")
set(PROJECT_BINARY_NAME "opendaq_qt_gui")  # The actual binary name from the main CMake project

# Detect architecture
if(CMAKE_OSX_ARCHITECTURES)
    # Use CMAKE_OSX_ARCHITECTURES if set
    if(CMAKE_OSX_ARCHITECTURES MATCHES "arm64")
        set(ARCH "arm64")
    elseif(CMAKE_OSX_ARCHITECTURES MATCHES "x86_64")
        set(ARCH "x86_64")
    else()
        set(ARCH "${CMAKE_OSX_ARCHITECTURES}")
    endif()
else()
    # Detect architecture from system
    execute_process(
        COMMAND uname -m
        OUTPUT_VARIABLE ARCH_RAW
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(ARCH_RAW MATCHES "arm64|aarch64")
        set(ARCH "arm64")
    else()
        set(ARCH "x86_64")
    endif()
endif()

# Path to the built application bundle
# This assumes the main project was built with -DBUILD_BUNDLE=ON
set(SOURCE_BUNDLE "${CMAKE_CURRENT_SOURCE_DIR}/../../build/bin/${PROJECT_BINARY_NAME}.app")

if(NOT EXISTS "${SOURCE_BUNDLE}")
    message(FATAL_ERROR
        "Application bundle not found at: ${SOURCE_BUNDLE}\n"
        "Please build the main project first with:\n"
        "  cmake -B build -DBUILD_BUNDLE=ON\n"
        "  cmake --build build"
    )
endif()

# Find Qt and macdeployqt
find_package(Qt6 REQUIRED COMPONENTS Core)
get_target_property(_qmake_executable Qt6::qmake IMPORTED_LOCATION)
get_filename_component(_qt_bin_dir "${_qmake_executable}" DIRECTORY)
find_program(MACDEPLOYQT_EXECUTABLE macdeployqt HINTS "${_qt_bin_dir}")

if(NOT MACDEPLOYQT_EXECUTABLE)
    message(FATAL_ERROR "macdeployqt not found. Cannot create deployable bundle.")
endif()

# Output paths
set(PACKAGE_DIR "${CMAKE_CURRENT_BINARY_DIR}/package")
set(BUNDLE_DIR "${PACKAGE_DIR}/${APP_NAME}.app")
set(DMG_FILE "${CMAKE_CURRENT_BINARY_DIR}/${APP_NAME}-${VERSION}-${ARCH}.dmg")

# Copy and deploy bundle
add_custom_target(prepare_bundle ALL
    COMMAND ${CMAKE_COMMAND} -E remove_directory "${PACKAGE_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${PACKAGE_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${SOURCE_BUNDLE}" "${BUNDLE_DIR}"
    COMMENT "Copying application bundle to package directory (renaming to '${APP_NAME}.app')..."
)

# Copy openDAQ dynamic libraries and modules
set(BUILD_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../build/bin")

# Find all .dylib files (libraries and modules)
file(GLOB OPENDAQ_LIBS "${BUILD_LIB_DIR}/*.dylib")

add_custom_target(copy_opendaq_libs ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory "${BUNDLE_DIR}/Contents/Frameworks"
    DEPENDS prepare_bundle
    COMMENT "Copying openDAQ libraries and modules to bundle..."
)

# Add copy commands for each library (to Frameworks)
foreach(lib ${OPENDAQ_LIBS})
    add_custom_command(TARGET copy_opendaq_libs POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${lib}" "${BUNDLE_DIR}/Contents/Frameworks/"
        COMMENT "Copying ${lib}"
    )
endforeach()

# Run macdeployqt to bundle Qt frameworks and plugins
add_custom_target(deploy_qt ALL
    COMMAND "${MACDEPLOYQT_EXECUTABLE}" "${BUNDLE_DIR}" -verbose=2
    DEPENDS copy_opendaq_libs
    COMMENT "Deploying Qt dependencies into bundle..."
)

# Fix library paths using install_name_tool
set(BUNDLE_EXECUTABLE "${BUNDLE_DIR}/Contents/MacOS/${PROJECT_BINARY_NAME}")
add_custom_target(fix_rpaths ALL
    COMMAND install_name_tool -add_rpath @executable_path/../Frameworks "${BUNDLE_EXECUTABLE}" || true
    DEPENDS deploy_qt
    COMMENT "Adding rpath to executable..."
    VERBATIM
)

# Re-sign the bundle with ad-hoc signature (for development)
add_custom_target(codesign_bundle ALL
    COMMAND codesign --force --deep --sign - "${BUNDLE_DIR}"
    DEPENDS fix_rpaths
    COMMENT "Re-signing bundle with ad-hoc signature..."
    VERBATIM
)

# Create DMG
add_custom_target(create_dmg
    COMMAND ${CMAKE_COMMAND} -E remove -f "${DMG_FILE}"
    COMMAND hdiutil create
            -volname "${APP_NAME}"
            -srcfolder "${PACKAGE_DIR}"
            -ov
            -format UDBZ
            "${DMG_FILE}"
    DEPENDS codesign_bundle
    COMMENT "Creating DMG: ${DMG_FILE}"
    BYPRODUCTS "${DMG_FILE}"
)

message(STATUS "==============================================")
message(STATUS "DMG Package Configuration")
message(STATUS "==============================================")
message(STATUS "Source bundle: ${SOURCE_BUNDLE}")
message(STATUS "Package dir:   ${PACKAGE_DIR}")
message(STATUS "Architecture:  ${ARCH}")
message(STATUS "Output DMG:    ${DMG_FILE}")
message(STATUS "macdeployqt:   ${MACDEPLOYQT_EXECUTABLE}")
message(STATUS "")
message(STATUS "Build steps:")
message(STATUS "  1. cd package/mac && cmake -B ../../build/package")
message(STATUS "  2. cmake --build ../../build/package")
message(STATUS "  3. cmake --build ../../build/package --target create_dmg")
message(STATUS "")
message(STATUS "Or use the script:")
message(STATUS "  ./package/mac/build_dmg.sh")
message(STATUS "==============================================")
