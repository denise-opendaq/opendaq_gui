set(LIB_NAME qt_module)
set(MODULE_HEADERS_DIR ../include/${TARGET_FOLDER_NAME})

# Enable Qt MOC, UIC, RCC
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Note: We don't disable Qt keywords here for Qt compatibility
# add_compile_definitions(QT_NO_KEYWORDS)

# Find Qt6
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Charts)

# Source files
set(SRC_Include
    common.h
    version.h
    module_dll.h
    opendaq_qt_module_impl.h
    qt_plotter_fb_impl.h
)

set(SRC_Srcs
    module_dll.cpp
    opendaq_qt_module_impl.cpp
    qt_plotter_fb_impl.cpp
)

prepend_include(${TARGET_FOLDER_NAME} SRC_Include)

source_group("module" FILES ${MODULE_HEADERS_DIR}/common.h
                            ${MODULE_HEADERS_DIR}/version.h
                            ${MODULE_HEADERS_DIR}/module_dll.h
                            ${MODULE_HEADERS_DIR}/opendaq_qt_module_impl.h
                            ${MODULE_HEADERS_DIR}/qt_plotter_fb_impl.h
                            module_dll.cpp
                            opendaq_qt_module_impl.cpp
                            qt_plotter_fb_impl.cpp
)

add_library(${LIB_NAME} SHARED ${SRC_Include}
                               ${SRC_Srcs}
)

add_library(daq::${LIB_NAME} ALIAS ${LIB_NAME})

target_link_libraries(${LIB_NAME}
    PUBLIC
        daq::opendaq
        daq::coretypes
        daq::coreobjects
    PRIVATE
        Qt6::Core
        Qt6::Widgets
        Qt6::Charts
)

target_include_directories(${LIB_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/../include>
        $<INSTALL_INTERFACE:include>
)

# Undefine QT_NO_KEYWORDS for this module for Qt compatibility
target_compile_options(${LIB_NAME} PRIVATE -UQT_NO_KEYWORDS)

opendaq_set_module_properties(${LIB_NAME} ${PROJECT_VERSION_MAJOR})
create_version_header(${LIB_NAME})