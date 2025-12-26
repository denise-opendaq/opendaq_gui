cmake_minimum_required(VERSION 3.16)

project(property VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Header files
set(HEADERS
    include/property/base_property_item.h
    include/property/object_property_item.h
    include/property/int_property_item.h
    include/property/bool_property_item.h
    include/property/dict_property_item.h
    include/property/list_property_item.h
    include/property/struct_property_item.h
    include/property/enumeration_property_item.h
    include/property/property_factory.h
    include/property/default_core_type.h
    include/property/coretypes/base_core_type_handler.h
    include/property/coretypes/bool_core_type_handler.h
    include/property/coretypes/enumeration_core_type_handler.h
    include/property/coretypes/default_core_type_handler.h
    include/property/coretypes/core_type_factory.h
)

# Source files
set(SOURCES
    src/base_property_item.cpp
    src/int_property_item.cpp
    src/object_property_item.cpp
    src/bool_property_item.cpp
    src/dict_property_item.cpp
    src/list_property_item.cpp
    src/struct_property_item.cpp
    src/enumeration_property_item.cpp
    src/property_factory.cpp
    src/coretypes/base_core_type_handler.cpp
    src/coretypes/bool_core_type_handler.cpp
    src/coretypes/enumeration_core_type_handler.cpp
    src/coretypes/default_core_type_handler.cpp
    src/coretypes/core_type_factory.cpp
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
        widgets
        daq::coreobjects
        daq::coretypes
        daq::opendaq
)