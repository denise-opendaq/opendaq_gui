cmake_minimum_required(VERSION 3.16)

project(opendaq_qt_gui_package)

# Configuration
set(APP_NAME "OpenDAQ Qt GUI")
set(VERSION "1.0")
set(PROJECT_BINARY_NAME "opendaq_qt_gui")

# Detect architecture
if(CMAKE_SYSTEM_PROCESSOR MATCHES "AMD64|x86_64|X64")
    set(ARCH "x64")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "ARM64|aarch64")
    set(ARCH "arm64")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "ARM")
    set(ARCH "arm")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "i[3-6]86|X86")
    set(ARCH "x86")
else()
    # Fallback: try to detect from environment
    if(DEFINED ENV{PROCESSOR_ARCHITECTURE})
        if("$ENV{PROCESSOR_ARCHITECTURE}" MATCHES "AMD64")
            set(ARCH "x64")
        elseif("$ENV{PROCESSOR_ARCHITECTURE}" MATCHES "ARM64")
            set(ARCH "arm64")
        else()
            set(ARCH "x64")  # Default to x64 for Windows
        endif()
    else()
        set(ARCH "x64")  # Default to x64 for Windows
    endif()
endif()

set(INSTALLER_NAME "${APP_NAME}-${VERSION}-Windows-${ARCH}")

# Path to the built executable
# Try Release first (for multi-config generators like Visual Studio), then single-config path
set(SOURCE_EXECUTABLE_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/../../build/bin/Release/${PROJECT_BINARY_NAME}.exe")
set(SOURCE_EXECUTABLE_SINGLE "${CMAKE_CURRENT_SOURCE_DIR}/../../build/bin/${PROJECT_BINARY_NAME}.exe")

if(EXISTS "${SOURCE_EXECUTABLE_RELEASE}")
    set(SOURCE_EXECUTABLE "${SOURCE_EXECUTABLE_RELEASE}")
    set(BUILD_LIB_DIR_ACTUAL "${CMAKE_CURRENT_SOURCE_DIR}/../../build/bin/Release")
elseif(EXISTS "${SOURCE_EXECUTABLE_SINGLE}")
    set(SOURCE_EXECUTABLE "${SOURCE_EXECUTABLE_SINGLE}")
    set(BUILD_LIB_DIR_ACTUAL "${CMAKE_CURRENT_SOURCE_DIR}/../../build/bin")
else()
    message(FATAL_ERROR
        "Executable not found at:\n"
        "  ${SOURCE_EXECUTABLE_RELEASE}\n"
        "  ${SOURCE_EXECUTABLE_SINGLE}\n"
        "Please build the main project first with:\n"
        "  cmake --preset opendaq-qt-gui -DCMAKE_BUILD_TYPE=Release\n"
        "  cmake --build --preset opendaq-qt-gui --config Release"
    )
endif()

# Find Qt and windeployqt
find_package(Qt6 REQUIRED COMPONENTS Core)
get_target_property(_qmake_executable Qt6::qmake IMPORTED_LOCATION)
get_filename_component(_qt_bin_dir "${_qmake_executable}" DIRECTORY)
find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${_qt_bin_dir}")

if(NOT WINDEPLOYQT_EXECUTABLE)
    message(FATAL_ERROR "windeployqt not found. Cannot create deployable package.")
endif()

# Output paths
set(PACKAGE_DIR "${CMAKE_CURRENT_BINARY_DIR}/package")
set(INSTALL_DIR "${PACKAGE_DIR}/${APP_NAME}")
set(INSTALLER_FILE "${CMAKE_CURRENT_BINARY_DIR}/${INSTALLER_NAME}.exe")

# Copy executable and dependencies
add_custom_target(prepare_package ALL
    COMMAND ${CMAKE_COMMAND} -E remove_directory "${PACKAGE_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${INSTALL_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy "${SOURCE_EXECUTABLE}" "${INSTALL_DIR}/${PROJECT_BINARY_NAME}.exe"
    COMMENT "Copying executable to package directory..."
)

# Copy openDAQ dynamic libraries
file(GLOB OPENDAQ_LIBS "${BUILD_LIB_DIR_ACTUAL}/*.dll")

add_custom_target(copy_opendaq_libs ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory "${INSTALL_DIR}"
    DEPENDS prepare_package
    COMMENT "Copying openDAQ libraries to package directory..."
)

foreach(lib ${OPENDAQ_LIBS})
    add_custom_command(TARGET copy_opendaq_libs POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${lib}" "${INSTALL_DIR}/"
        COMMENT "Copying ${lib}"
    )
endforeach()

# Run windeployqt to bundle Qt dependencies
add_custom_target(deploy_qt ALL
    COMMAND "${WINDEPLOYQT_EXECUTABLE}" "${INSTALL_DIR}/${PROJECT_BINARY_NAME}.exe" --release --compiler-runtime
    DEPENDS copy_opendaq_libs
    COMMENT "Deploying Qt dependencies into package..."
)

# Find NSIS for installer creation
find_program(NSIS_EXECUTABLE makensis PATHS
    "C:/Program Files (x86)/NSIS"
    "C:/Program Files/NSIS"
    ENV NSIS_PATH
)

if(NOT NSIS_EXECUTABLE)
    message(WARNING "NSIS not found. Installer will not be created.")
    message(STATUS "Install NSIS from: https://nsis.sourceforge.io/Download")
else()
    # Create NSIS installer script
    set(NSIS_SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/installer.nsi")
    
    # Convert paths to Windows format for NSIS (use forward slashes for NSIS)
    file(TO_CMAKE_PATH "${INSTALL_DIR}" INSTALL_DIR_CMAKE)
    file(TO_CMAKE_PATH "${INSTALLER_FILE}" INSTALLER_FILE_CMAKE)
    string(REPLACE "\\" "/" INSTALL_DIR_NATIVE "${INSTALL_DIR_CMAKE}")
    string(REPLACE "\\" "/" INSTALLER_FILE_NATIVE "${INSTALLER_FILE_CMAKE}")
    
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/installer.nsi.in"
        "${NSIS_SCRIPT}"
        @ONLY
    )
    
    # Create installer
    add_custom_target(create_installer
        COMMAND "${NSIS_EXECUTABLE}" "${NSIS_SCRIPT}"
        DEPENDS deploy_qt
        COMMENT "Creating NSIS installer: ${INSTALLER_FILE}"
        BYPRODUCTS "${INSTALLER_FILE}"
    )
endif()

message(STATUS "==============================================")
message(STATUS "Windows Installer Package Configuration")
message(STATUS "==============================================")
message(STATUS "Source executable: ${SOURCE_EXECUTABLE}")
message(STATUS "Package dir:       ${PACKAGE_DIR}")
message(STATUS "Architecture:      ${ARCH}")
message(STATUS "Output installer:  ${INSTALLER_FILE}")
message(STATUS "windeployqt:       ${WINDEPLOYQT_EXECUTABLE}")
message(STATUS "NSIS:              ${NSIS_EXECUTABLE}")
message(STATUS "")
message(STATUS "Build steps:")
message(STATUS "  1. cd package/windows && cmake -B ../../build/package")
message(STATUS "  2. cmake --build ../../build/package")
message(STATUS "  3. cmake --build ../../build/package --target create_installer")
message(STATUS "==============================================")

