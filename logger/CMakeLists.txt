# Logger library
add_library(logger STATIC
    src/qt_text_edit_sink.cpp
)

# Qt6 setup
set(CMAKE_AUTOMOC ON)

target_include_directories(logger PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(logger PUBLIC
    Qt6::Widgets
    daq::opendaq
    spdlog
)
