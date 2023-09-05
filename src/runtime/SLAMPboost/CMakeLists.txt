file(GLOB SRCS
    "*.cpp"
)

# Compilation flags
# set_source_files_properties(${SRCS} PROPERTIES COMPILE_FLAGS "-Wl,-save-temps -std=c++20 -Wno-inline -O3 -fexceptions")# -emit-llvm")
set_source_files_properties(${SRCS} PROPERTIES COMPILE_FLAGS "-std=c++20 -Wno-inline -O3 -fexceptions")

# if RUNTIME_LTO is set, add LTO flags
if(RUNTIME_LTO)
    set_source_files_properties(${SRCS} PROPERTIES COMPILE_FLAGS "-flto")
endif()

set(PassName "slamp_hooks_boost")

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
