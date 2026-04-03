include(FetchContent)

FetchContent_Declare(
    openDAQ
    GIT_REPOSITORY  https://github.com/openDAQ/openDAQ.git
    GIT_TAG         7323c81b702d1cddbf135904fa7fc0bee64bff2a
    OVERRIDE_FIND_PACKAGE
)

FetchContent_MakeAvailable(openDAQ)
find_package(openDAQ REQUIRED)