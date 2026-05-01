# Single source of version for the main project and all package builds.
# The root CMakeLists.txt uses this via project(... VERSION ${PROJECT_VERSION}).
# cmake/Packing.cmake includes this and sets VERSION ${PROJECT_VERSION}.
set(PROJECT_VERSION "1.2.2")
