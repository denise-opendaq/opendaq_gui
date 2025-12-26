cmake_minimum_required(VERSION 3.16)

project(widgets VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Header files
set(HEADERS
    include/widgets/signal_value_widget.h
    include/widgets/property_object_view.h
    include/widgets/input_port_widget.h
    include/widgets/input_port_signal_selector.h
    include/widgets/function_block_widget.h
)

# Source files
set(SOURCES
    src/signal_value_widget.cpp
    src/property_object_view.cpp
    src/input_port_widget.cpp
    src/input_port_signal_selector.cpp
    src/function_block_widget.cpp
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
        property
        daq::coreobjects
        daq::coretypes
        daq::opendaq
)
