cmake_minimum_required(VERSION 3.16)

project(opendaq_qt_gui VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# for openDAQ - suppress GCC 15 errors in openDAQ dependencies
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options(-Wno-error=deprecated-declarations -Wno-error=stringop-overflow -Wno-error=overloaded-virtual)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    add_compile_options(-Wno-error=deprecated-declarations -Wno-error=overloaded-virtual)
elseif(MSVC)
    # MSVC equivalent flags (only for C/CXX, not MASM)
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:/wd4996>)  # disable deprecated declarations warning
endif()

add_subdirectory(external)

# Qt6 setup - enable automatic MOC, UIC, RCC for this project
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Disable Qt keywords (signals/slots) globally to avoid conflicts with openDAQ
# openDAQ uses 'signals' as parameter/variable names
# Use Q_SIGNALS, Q_SLOTS, Q_EMIT instead in Qt code
add_compile_definitions(QT_NO_KEYWORDS)

# Find Qt6 (fetched or system-installed in external/qt)
find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

add_subdirectory(context)
add_subdirectory(logger)
add_subdirectory(coretypes)
add_subdirectory(property)
add_subdirectory(widgets)
add_subdirectory(dialogs)
add_subdirectory(utils)
add_subdirectory(component)
add_subdirectory(opendaq_qt_module)

# Source files
set(SOURCES
    main.cpp
    MainWindow.cpp
    DetachableTabWidget.cpp
    DetachedWindow.cpp
    DropOverlay.cpp
)

set(HEADERS
    MainWindow.h
    DetachableTabWidget.h
    DetachedWindow.h
    DropOverlay.h
)

# Create executable
add_executable(${PROJECT_NAME}
    ${SOURCES}
    ${HEADERS}
)

# Link Qt6 libraries
target_link_libraries(${PROJECT_NAME}
    PRIVATE
        utils
        Qt6::Core
        Qt6::Widgets
        context
        logger
        component
        dialogs
        property
        widgets
        daq::qt_module
        daq::coreobjects
        daq::coretypes
        daq::opendaq
)

# Option to build as bundle (for packaging - macOS .app bundle or Linux with bundled Qt)
option(BUILD_BUNDLE "Build as platform bundle (macOS .app or Linux with bundled Qt)" OFF)

# Option for extra module path (for bundle packaging)
set(EXTRA_MODULE_PATH "" CACHE STRING "Additional path for openDAQ modules (relative to executable)")

# Pass extra module path to the application via compile definition
if(EXTRA_MODULE_PATH)
    target_compile_definitions(${PROJECT_NAME} PRIVATE EXTRA_MODULE_PATH="${EXTRA_MODULE_PATH}")
endif()

# Platform-specific bundle configuration
if(BUILD_BUNDLE)
    if(APPLE)
        # macOS bundle configuration
        set_target_properties(${PROJECT_NAME} PROPERTIES
            MACOSX_BUNDLE TRUE
            MACOSX_BUNDLE_GUI_IDENTIFIER "com.opendaq.qt-gui"
            MACOSX_BUNDLE_BUNDLE_NAME "OpenDAQ Qt GUI"
            MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
            MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        )
    elseif(UNIX)
        # Linux bundle - set RPATH to find bundled libraries
        set_target_properties(${PROJECT_NAME} PROPERTIES
            BUILD_RPATH "\$ORIGIN/../lib/opendaq-qt-gui:\$ORIGIN/../lib/opendaq-qt-gui/qt6"
            INSTALL_RPATH "\$ORIGIN/../lib/opendaq-qt-gui:\$ORIGIN/../lib/opendaq-qt-gui/qt6"
        )
    endif()
else()
    # Regular executable (default)
    if(APPLE)
        set_target_properties(${PROJECT_NAME} PROPERTIES
            MACOSX_BUNDLE FALSE
        )
    endif()
endif()
