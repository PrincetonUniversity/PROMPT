file(GLOB SRCS
    # "CustomSend_all.cpp"
    # "CustomSend_ol.cpp"
    # "CustomSend_lv.cpp"
    # "CustomSend_pt.cpp"
    "CustomSend.cpp"
    # "*.c"
)

# add CMAKE_PREFIX_PATH
set(CMAKE_C_COMPILER "clang")
set(CMAKE_CXX_COMPILER "clang++")

# Compilation flags
# set_source_files_properties(${SRCS} PROPERTIES COMPILE_FLAGS "-Wl,-save-temps -std=c++17 -Wno-inline -O3 -fexceptions")# -emit-llvm")
# set C++ flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -g")
set_source_files_properties(${SRCS} PROPERTIES COMPILE_FLAGS "-Wno-inline -O3 -g -fexceptions")# -emit-llvm")

# if RUNTIME_LTO is set, add LTO flags
if(RUNTIME_LTO)
    set_source_files_properties(${SRCS} PROPERTIES COMPILE_FLAGS "-flto")
endif()
set(PassName "slamp_hooks_custom")

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Boost REQUIRED COMPONENTS system)
include_directories(${Boost_INCLUDE_DIRS})


# list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
#include(HandleLLVMOptions)
# include(AddLLVM)


include_directories(./)

add_library(${PassName} STATIC ${SRCS})
target_link_libraries(${PassName} ${Boost_LIBRARIES})

# install this
install(TARGETS ${PassName} DESTINATION ${CMAKE_INSTALL_PREFIX}/runtime)
