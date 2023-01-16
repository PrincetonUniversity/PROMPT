file(GLOB SRCS
    "*.cpp"
)
set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} "/u/ziyangx/test/boost/boost_1_80_0/install/lib/cmake")

# add CMAKE_PREFIX_PATH 
set(CMAKE_C_COMPILER "clang")
set(CMAKE_CXX_COMPILER "clang++")

# Compilation flags
# set_source_files_properties(${SRCS} PROPERTIES COMPILE_FLAGS "-Wl,-save-temps -std=c++17 -Wno-inline -O3 -fexceptions")# -emit-llvm")
set_source_files_properties(${SRCS} PROPERTIES COMPILE_FLAGS "-flto -std=c++17 -Wno-inline -O3 -fexceptions")# -emit-llvm")
set(PassName "slamp_hooks_boost")

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Boost 1.80.0 REQUIRED COMPONENTS system)
include_directories(${Boost_INCLUDE_DIRS})


# list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
#include(HandleLLVMOptions)
# include(AddLLVM)


include_directories(./
        /u/ziyangx/test/boost/boost_1_80_0/install/include)

add_library(${PassName} STATIC ${SRCS})
target_link_libraries(${PassName} ${Boost_LIBRARIES})

# install this
install(TARGETS ${PassName} DESTINATION ${CMAKE_INSTALL_PREFIX}/runtime)
