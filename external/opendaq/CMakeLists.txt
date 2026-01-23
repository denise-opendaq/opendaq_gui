include(FetchContent)

FetchContent_Declare(
    openDAQ
    GIT_REPOSITORY  https://github.com/openDAQ/openDAQ.git
    GIT_TAG         origin/sync_component_rework
    OVERRIDE_FIND_PACKAGE
)

FetchContent_MakeAvailable(openDAQ)
find_package(openDAQ REQUIRED)