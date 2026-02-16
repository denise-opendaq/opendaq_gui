# Qt6 setup - enable automatic MOC, UIC, RCC
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Option to use system-installed Qt or fetch from git
option(USE_SYSTEM_QT "Use system-installed Qt instead of fetching from git" ON)

set(QT_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/_deps/qt6-install")

if(USE_SYSTEM_QT)
    message(STATUS "Using system-installed Qt6")
    find_package(Qt6 REQUIRED COMPONENTS Core Widgets Charts)
else()
    message(STATUS "Fetching and building Qt6 from git (first run may take 30-60 minutes)")

    # Check if we already built and installed Qt (including Charts) in a previous run
    set(QT_CACHE_VALID FALSE)
    if(EXISTS "${QT_INSTALL_PREFIX}/lib/cmake/Qt6/Qt6Config.cmake")
        # Validate via find_package so Charts is really available
        set(QT_SAVED_PREFIX_PATH "${CMAKE_PREFIX_PATH}")
        set(CMAKE_PREFIX_PATH "${QT_INSTALL_PREFIX};${CMAKE_PREFIX_PATH}")
        find_package(Qt6 QUIET COMPONENTS Core Widgets Charts)
        set(CMAKE_PREFIX_PATH "${QT_SAVED_PREFIX_PATH}")
        if(Qt6_FOUND)
            set(QT_CACHE_VALID TRUE)
            message(STATUS "Using previously built Qt6 (with Charts) at ${QT_INSTALL_PREFIX}")
        endif()
    endif()
    if(NOT QT_CACHE_VALID AND EXISTS "${QT_INSTALL_PREFIX}/lib/cmake/Qt6/Qt6Config.cmake")
        message(STATUS "Existing Qt6 install missing Charts; removing and rebuilding...")
        file(REMOVE_RECURSE "${QT_INSTALL_PREFIX}")
        if(EXISTS "${CMAKE_BINARY_DIR}/_deps/qt6-build")
            file(REMOVE_RECURSE "${CMAKE_BINARY_DIR}/_deps/qt6-build")
        endif()
    endif()
    if(QT_CACHE_VALID)
        list(PREPEND CMAKE_PREFIX_PATH "${QT_INSTALL_PREFIX}")
        set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH}" CACHE PATH "Prefix path for find_package" FORCE)
    else()
        find_package(Git QUIET)
        if(NOT GIT_EXECUTABLE)
            set(GIT_EXECUTABLE "git")
        endif()
        include(FetchContent)

        FetchContent_Declare(
            Qt6
            GIT_REPOSITORY  https://github.com/qt/qt5
            GIT_TAG         v6.10.2
            GIT_SHALLOW     TRUE
            GIT_SUBMODULES  "qtbase;qtcharts"
        )

        FetchContent_Populate(Qt6)

        # Ensure qtcharts submodule is present (FetchContent may not init multiple submodules)
        message(STATUS "Initializing qtcharts submodule in ${qt6_SOURCE_DIR}...")
        execute_process(
            COMMAND ${GIT_EXECUTABLE} submodule update --init qtcharts
            WORKING_DIRECTORY ${qt6_SOURCE_DIR}
            RESULT_VARIABLE QT_SUBMODULE_RESULT
            OUTPUT_VARIABLE QT_SUBMODULE_OUTPUT
            ERROR_VARIABLE QT_SUBMODULE_ERROR
        )
        if(NOT QT_SUBMODULE_RESULT EQUAL 0)
            message(FATAL_ERROR "qtcharts submodule init failed (${QT_SUBMODULE_RESULT}):\n${QT_SUBMODULE_OUTPUT}\n${QT_SUBMODULE_ERROR}")
        endif()
        if(NOT EXISTS "${qt6_SOURCE_DIR}/qtcharts/CMakeLists.txt")
            message(FATAL_ERROR "qtcharts source missing at ${qt6_SOURCE_DIR}/qtcharts - cannot build Qt6Charts")
        endif()

        set(QT_BUILD_DIR "${CMAKE_BINARY_DIR}/_deps/qt6-build")
        if(EXISTS "${QT_BUILD_DIR}/CMakeCache.txt")
            file(READ "${QT_BUILD_DIR}/CMakeCache.txt" QT_CACHE_CONTENT)
            if(NOT QT_CACHE_CONTENT MATCHES "QT_BUILD_SUBMODULES.*qtcharts")
                message(STATUS "Removing stale qt6-build (was built without qtcharts)...")
                file(REMOVE_RECURSE "${QT_BUILD_DIR}")
            endif()
        endif()
        file(MAKE_DIRECTORY ${QT_BUILD_DIR})

        # Use Qt's configure script so -submodules qtbase,qtcharts is handled correctly
        message(STATUS "Configuring Qt6 in ${QT_BUILD_DIR} (submodules: qtbase, qtcharts)...")
        if(CMAKE_BUILD_TYPE STREQUAL "Debug")
            set(QT_CONFIGURE_DEBUG "--debug")
        else()
            set(QT_CONFIGURE_DEBUG "--release")
        endif()
        execute_process(
            COMMAND ${qt6_SOURCE_DIR}/configure
                    -prefix ${QT_INSTALL_PREFIX}
                    -submodules qtbase,qtcharts
                    -shared
                    -nomake examples
                    -nomake tests
                    ${QT_CONFIGURE_DEBUG}
            WORKING_DIRECTORY ${QT_BUILD_DIR}
            RESULT_VARIABLE QT_CONFIGURE_RESULT
        )
        if(NOT QT_CONFIGURE_RESULT EQUAL 0)
            message(FATAL_ERROR "Qt6 configure failed (exit code ${QT_CONFIGURE_RESULT})")
        endif()

        message(STATUS "Building Qt6 (this may take 30-60 minutes)...")
        execute_process(
            COMMAND ${CMAKE_COMMAND} --build ${QT_BUILD_DIR} --parallel
            RESULT_VARIABLE QT_BUILD_RESULT
        )
        if(NOT QT_BUILD_RESULT EQUAL 0)
            message(FATAL_ERROR "Qt6 build failed (exit code ${QT_BUILD_RESULT})")
        endif()

        message(STATUS "Installing Qt6 to ${QT_INSTALL_PREFIX}...")
        execute_process(
            COMMAND ${CMAKE_COMMAND} --install ${QT_BUILD_DIR}
            RESULT_VARIABLE QT_INSTALL_RESULT
        )
        if(NOT QT_INSTALL_RESULT EQUAL 0)
            message(FATAL_ERROR "Qt6 install failed")
        endif()

        list(PREPEND CMAKE_PREFIX_PATH "${QT_INSTALL_PREFIX}")
        set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH}" CACHE PATH "Prefix path for find_package" FORCE)
    endif()
endif()
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Charts)
