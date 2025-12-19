# Qt6 setup - enable automatic MOC, UIC, RCC
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Option to use system-installed Qt or fetch from git
option(USE_SYSTEM_QT "Use system-installed Qt instead of fetching from git" ON)

if(USE_SYSTEM_QT)
    message(STATUS "Using system-installed Qt6")
else()
    message(STATUS "Fetching Qt6 from git repository (this may take a long time)")

    include(FetchContent)

    FetchContent_Declare(
        Qt6
        GIT_REPOSITORY  https://code.qt.io/qt/qt5.git
        GIT_TAG         v6.8.1
        GIT_SHALLOW     TRUE
        GIT_SUBMODULES  qtbase qttools
        OVERRIDE_FIND_PACKAGE
    )

    set(QT_BUILD_EXAMPLES OFF CACHE BOOL "Build Qt examples" FORCE)
    set(QT_BUILD_TESTS OFF CACHE BOOL "Build Qt tests" FORCE)
    set(FEATURE_gui ON CACHE BOOL "Build Qt GUI" FORCE)
    set(FEATURE_widgets ON CACHE BOOL "Build Qt Widgets" FORCE)

    FetchContent_MakeAvailable(Qt6)
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Widgets)