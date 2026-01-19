include(FetchContent)

FetchContent_Declare(
    openDAQ
    GIT_REPOSITORY  https://github.com/openDAQ/openDAQ.git
    GIT_TAG         v3.30.0
    OVERRIDE_FIND_PACKAGE
)

FetchContent_MakeAvailable(openDAQ)
find_package(openDAQ REQUIRED)