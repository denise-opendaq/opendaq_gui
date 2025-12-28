message(STATUS "Including openDAQ library")
add_subdirectory(opendaq)

message(STATUS "Including QT library.")
add_subdirectory(qt)

message(STATUS "Including QCustomPlot library.")
add_subdirectory(qcustomplot)