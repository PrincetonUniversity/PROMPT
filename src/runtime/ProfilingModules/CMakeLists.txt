# Compile into a library with files DependenceModule.cpp PointsToModule.cpp
# LoadedValueModule.cpp ObjectLifetimeModule.cpp

# set C++17
set(CMAKE_CXX_STANDARD 17)

# if RUNTIME_LTO is set, then add -flto to CMAKE_CXX_FLAGS and LINK_FLAGS
if(RUNTIME_LTO)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -flto")
  set(LINK_FLAGS "${LINK_FLAGS} -flto")
endif()

file(GLOB PRIVATEER_SOURCES "privateer/*.cpp")
add_library(
  ProfilingModules STATIC
  DependenceModule.cpp
  WholeProgramDependenceModule.cpp
  PointsToModule.cpp
  LoadedValueModule.cpp
  ObjectLifetimeModule.cpp
  PrivateerProfiler.cpp
  ${PRIVATEER_SOURCES})
