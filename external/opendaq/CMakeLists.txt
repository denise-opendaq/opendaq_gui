include(FetchContent)

# Set openDAQ options before FetchContent_MakeAvailable
set(OPENDAQ_ENABLE_TESTS OFF CACHE BOOL "Disable openDAQ tests" FORCE)
set(DAQMODULES_OPENDAQ_CLIENT_MODULE ON CACHE STRING "Enable openDAQ client module" FORCE)
set(DAQMODULES_OPENDAQ_SERVER_MODULE ON CACHE BOOL "Enable openDAQ server module" FORCE)
set(OPENDAQ_ENABLE_NATIVE_STREAMING ON CACHE BOOL "Enable openDAQ native streaming" FORCE)
set(OPENDAQ_ENABLE_OPCUA ON CACHE BOOL "Enable openDAQ OPC UA module" FORCE)
set(DAQMODULES_REF_DEVICE_MODULE ON CACHE BOOL "Enable openDAQ reference device module" FORCE)
set(DAQMODULES_REF_FB_MODULE ON CACHE BOOL "Enable openDAQ reference function block module" FORCE)
set(DAQMODULES_REF_FB_MODULE_ENABLE_RENDERER ON CACHE BOOL "Enable openDAQ reference function block module renderer" FORCE)

FetchContent_Declare(
    openDAQ
    GIT_REPOSITORY  https://github.com/openDAQ/openDAQ.git
    GIT_TAG         v3.30.0
    OVERRIDE_FIND_PACKAGE
)

FetchContent_MakeAvailable(openDAQ)
find_package(openDAQ REQUIRED)