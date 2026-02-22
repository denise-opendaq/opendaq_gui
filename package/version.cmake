# Single source of version for the main project and all package builds.
# The root CMakeLists.txt uses this via project(... VERSION ${PROJECT_VERSION}).
# Package CMake (mac, linux, windows) include this and set(VERSION ${PROJECT_VERSION}).
set(PROJECT_VERSION "1.2")
