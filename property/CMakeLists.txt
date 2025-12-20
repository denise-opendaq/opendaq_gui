cmake_minimum_required(VERSION 3.16)

project(property VERSION 1.0 LANGUAGES CXX)

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
    include/property/property_object_view.h
    include/property/base_property_item.h
    include/property/object_property_item.h
    include/property/int_property_item.h
    include/property/bool_property_item.h
    include/property/dict_property_item.h
    include/property/property_factory.h
)

# Source files
set(SOURCES
    src/property_object_view.cpp
    src/int_property_item.cpp
    src/object_property_item.cpp
    src/bool_property_item.cpp
    src/dict_property_item.cpp
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
        $<INSTALL_INTERFACE:include>
)

# Link Qt6 libraries
target_link_libraries(${PROJECT_NAME}
    PUBLIC
        Qt6::Core
        Qt6::Widgets
        daq::coreobjects
        daq::coretypes
        daq::opendaq
)

# If you have openDAQ dependencies, add them here
# target_link_libraries(${PROJECT_NAME}
#     INTERFACE
#         daq::coreobjects
#         daq::coretypes
#         daq::opendaq
# )
