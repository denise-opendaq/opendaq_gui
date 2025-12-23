cmake_minimum_required(VERSION 3.16)

project(dialogs VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt6 setup
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Find Qt6
find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

# Disable Qt keywords (signals/slots) to avoid conflicts with openDAQ
# openDAQ uses 'signals' as parameter/variable names
# Use Q_SIGNALS, Q_SLOTS, Q_EMIT instead in Qt code
add_compile_definitions(QT_NO_KEYWORDS)

# Header files
set(HEADERS
    include/dialogs/add_device_dialog.h
    include/dialogs/add_device_config_dialog.h
    include/dialogs/add_function_block_dialog.h
    include/dialogs/add_server_dialog.h
)

# Source files
set(SOURCES
    src/add_device_dialog.cpp
    src/add_device_config_dialog.cpp
    src/add_function_block_dialog.cpp
    src/add_server_dialog.cpp
)

# Create library
add_library(${PROJECT_NAME} STATIC
    ${HEADERS}
    ${SOURCES}
)

# Include directories
target_include_directories(${PROJECT_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../>
        $<INSTALL_INTERFACE:include>
)

# Link Qt6 libraries
target_link_libraries(${PROJECT_NAME}
    PUBLIC
        Qt6::Core
        Qt6::Widgets
        context
        widgets
        property
        daq::coreobjects
        daq::coretypes
        daq::opendaq
)

