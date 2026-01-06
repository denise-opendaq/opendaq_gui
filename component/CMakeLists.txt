cmake_minimum_required(VERSION 3.16)

project(component VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Header files
set(HEADERS
    include/component/base_tree_element.h
    include/component/device_tree_element.h
    include/component/component_tree_element.h
    include/component/folder_tree_element.h
    include/component/devices_folder_tree_element.h
    include/component/function_blocks_folder_tree_element.h
    include/component/servers_folder_tree_element.h
    include/component/function_block_tree_element.h
    include/component/server_tree_element.h
    include/component/input_port_tree_element.h
    include/component/input_port_folder_tree_element.h
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
    src/devices_folder_tree_element.cpp
    src/function_blocks_folder_tree_element.cpp
    src/servers_folder_tree_element.cpp
    src/device_tree_element.cpp
    src/function_block_tree_element.cpp
    src/server_tree_element.cpp
    src/signal_tree_element.cpp
    src/input_port_tree_element.cpp
    src/input_port_folder_tree_element.cpp
    src/component_factory.cpp
    src/component.cpp
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
        dialogs
        property
        widgets
        daq::coreobjects
        daq::coretypes
        daq::opendaq
)
