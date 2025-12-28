include(FetchContent)

# QCustomPlot source (since there's no official CMake repository, we'll download directly)
FetchContent_Declare(
    qcustomplot
    URL https://www.qcustomplot.com/release/2.1.1/QCustomPlot-source.tar.gz
    URL_HASH SHA256=5e2d22dec779db8f01f357cbdb25e54fbcf971adaee75eae8d7ad2444487182f
)

FetchContent_MakeAvailable(qcustomplot)

# Create qcustomplot library
set(QCUSTOMPLOT_SOURCES
    ${qcustomplot_SOURCE_DIR}/qcustomplot.cpp
)

set(QCUSTOMPLOT_HEADERS
    ${qcustomplot_SOURCE_DIR}/qcustomplot.h
)

# Create library
add_library(qcustomplot STATIC
    ${QCUSTOMPLOT_SOURCES}
    ${QCUSTOMPLOT_HEADERS}
)

# Qt setup for qcustomplot
set_target_properties(qcustomplot PROPERTIES
    AUTOMOC ON
    CXX_STANDARD 17
)

# Find Qt6
find_package(Qt6 REQUIRED COMPONENTS Core Widgets PrintSupport)

target_link_libraries(qcustomplot
    PUBLIC
        Qt6::Core
        Qt6::Widgets
        Qt6::PrintSupport
)

target_include_directories(qcustomplot
    PUBLIC
        $<BUILD_INTERFACE:${qcustomplot_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
)

# Create alias for consistent naming
add_library(QCustomPlot::QCustomPlot ALIAS qcustomplot)
