cmake_minimum_required(VERSION 3.16)

project(context VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Header files
set(HEADERS
    include/context/AppContext.h
    include/context/UpdateScheduler.h
    include/context/QueuedEventHandler.h
    include/context/icon_provider.h
    include/context/gui_constants.h
)

# Source files
set(SOURCES
    src/AppContext.cpp
    src/UpdateScheduler.cpp
    src/QueuedEventHandler.cpp
    src/icon_provider.cpp
)

# Resource files
set(RESOURCES
    ${CMAKE_SOURCE_DIR}/icons/icons.qrc
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

# Link Qt6 libraries and openDAQ
target_link_libraries(${PROJECT_NAME}
    PUBLIC
        Qt6::Core
        Qt6::Widgets
        logger
        daq::coreobjects
        daq::coretypes
        daq::opendaq
)

