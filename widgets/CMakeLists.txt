cmake_minimum_required(VERSION 3.16)

project(widgets VERSION 1.0 LANGUAGES CXX)

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
    include/widgets/signal_value_widget.h
    include/widgets/property_object_view.h
)

# Source files
set(SOURCES
    src/signal_value_widget.cpp
    src/property_object_view.cpp
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
        property
        daq::coreobjects
        daq::coretypes
        daq::opendaq
)
