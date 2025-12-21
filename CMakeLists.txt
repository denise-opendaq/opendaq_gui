cmake_minimum_required(VERSION 3.16)

project(opendaq_qt_gui VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_subdirectory(external)
add_subdirectory(property)
add_subdirectory(widgets)
add_subdirectory(component)

# Find Qt6 (fetched or system-installed in external/qt)
find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

# Qt6 setup - enable automatic MOC, UIC, RCC for this project
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Disable Qt keywords (signals/slots) globally to avoid conflicts with openDAQ
# openDAQ uses 'signals' as parameter/variable names
# Use Q_SIGNALS, Q_SLOTS, Q_EMIT instead in Qt code
add_compile_definitions(QT_NO_KEYWORDS)

# Source files
set(SOURCES
    main.cpp
    AppContext.cpp
    UpdateScheduler.cpp
    MainWindow.cpp
    DetachableTabWidget.cpp
    DetachedWindow.cpp
    DropOverlay.cpp
)

set(HEADERS
    AppContext.h
    UpdateScheduler.h
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
        Qt6::Core
        Qt6::Widgets
        component
        property
        widgets
        daq::coreobjects
        daq::coretypes
        daq::opendaq
)

# Set output directory
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# For macOS bundle
if(APPLE)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        MACOSX_BUNDLE TRUE
    )
endif()
