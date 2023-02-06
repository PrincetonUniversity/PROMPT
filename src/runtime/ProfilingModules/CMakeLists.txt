# Compile into a library with files DependenceModule.cpp PointsToModule.cpp LoadedValueModule.cpp ObjectLifetimeModule.cpp
set(CMAKE_C_COMPILER "clang")
set(CMAKE_CXX_COMPILER "clang++")
set(CMAKE_LINKER "ld.gold")

# set C++17
set(CMAKE_CXX_STANDARD 17)

# if RUNTIME_LTO is set, then add -flto to CMAKE_CXX_FLAGS and LINK_FLAGS
if(RUNTIME_LTO)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -flto")
    set(LINK_FLAGS "${LINK_FLAGS} -flto")
endif()

add_library(ProfilingModules STATIC
    DependenceModule.cpp
    PointsToModule.cpp
    LoadedValueModule.cpp
    ObjectLifetimeModule.cpp
)
