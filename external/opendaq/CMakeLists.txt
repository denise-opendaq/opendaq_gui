include(FetchContent)

# fetch the cmake utilities first, as they are used by the other modules
FetchContent_Declare(
    opendaq-cmake-utils
    GIT_REPOSITORY https://github.com/openDAQ/opendaq-cmake-utils.git
    GIT_TAG        v1.0.1
    GIT_PROGRESS   ON
)
FetchContent_MakeAvailable(opendaq-cmake-utils)

# fetching the openDAQ 
FetchContent_Declare(
    openDAQ
    GIT_REPOSITORY  https://github.com/openDAQ/openDAQ.git
    GIT_TAG         be12629693fcbc1cdb22313968a23ec1af6d9052
    GIT_PROGRESS   ON
    GIT_SHALLOW    OFF
    SOURCE_SUBDIR  "INVALID"
    GIT_REMOTE_UPDATE_STRATEGY CHECKOUT
)

FetchContent_MakeAvailable(openDAQ)
include("${opendaq_SOURCE_DIR}/external/boost/Boost.cmake" OPTIONAL)

# fetching the LTStreamingModulesModern
if(OPENDAQ_ENABLE_WEBSOCKET_STREAMING AND (DAQMODULES_OPENDAQ_SERVER_MODULE OR DAQMODULES_OPENDAQ_CLIENT_MODULE))
    FetchContent_Declare(
        LtStreamingModulesModern
        GIT_REPOSITORY https://github.com/openDAQ/LTStreamingModulesModern.git
        GIT_TAG        29973836172ed53bc4b199436e5d732447dbf40d
        GIT_PROGRESS   ON
        GIT_SHALLOW    OFF
        SOURCE_SUBDIR  "INVALID"
        GIT_REMOTE_UPDATE_STRATEGY CHECKOUT
    )

    FetchContent_MakeAvailable(LtStreamingModulesModern)
    include("${ltstreamingmodulesmodern_SOURCE_DIR}/external/boost/Boost.cmake" OPTIONAL)
endif()

# fetching the OpcUaModules
if(OPENDAQ_ENABLE_OPCUA AND (DAQMODULES_OPENDAQ_SERVER_MODULE OR DAQMODULES_OPENDAQ_CLIENT_MODULE))
    FetchContent_Declare(
        OpcUaModules
        GIT_REPOSITORY https://github.com/openDAQ/OpcUaModules.git
        GIT_TAG        5bce4a3421e715354317f316947eb6a716aca01e
        GIT_PROGRESS   ON
        GIT_SHALLOW    OFF
        SOURCE_SUBDIR  "INVALID"
        GIT_REMOTE_UPDATE_STRATEGY CHECKOUT
    )

    FetchContent_MakeAvailable(OpcUaModules)
    include("${opcuamodules_SOURCE_DIR}/external/boost/Boost.cmake" OPTIONAL)
endif()

# add the subdirectories for the core and modules

add_subdirectory(${opendaq_SOURCE_DIR} ${opendaq_BINARY_DIR})