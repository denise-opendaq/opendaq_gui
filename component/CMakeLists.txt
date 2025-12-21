cmake_minimum_required(VERSION 3.16)

project(component VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt6 setup
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Find Qt6
find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

# Header files
set(HEADERS
    include/component/base_tree_element.h
    include/component/icon_provider.h
    include/component/device_tree_element.h
    include/component/component_tree_element.h
    include/component/folder_tree_element.h
    include/component/function_block_tree_element.h
    include/component/input_port_tree_element.h
    include/component/signal_tree_element.h
    include/component/component_factory.h
    include/component/component_tree_widget.h
)

# Source files
set(SOURCES
    src/base_tree_element.cpp
    src/component_tree_element.cpp
    src/component_tree_widget.cpp
    src/folder_tree_element.cpp
    src/device_tree_element.cpp
    src/function_block_tree_element.cpp
    src/signal_tree_element.cpp
    src/input_port_tree_element.cpp
    src/component_factory.cpp
    src/icon_provider.cpp
    src/component.cpp
)

# Resource files
set(RESOURCES
    icons/icons.qrc
)

# Create library
add_library(${PROJECT_NAME} STATIC
    ${HEADERS}
    ${SOURCES}
    ${RESOURCES}
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
        property
        widgets
        daq::coreobjects
        daq::coretypes
        daq::opendaq
)
