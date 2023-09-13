# Link with LLVM
include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Prepare the pass to be included in the source tree
list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(AddLLVM)

# Programming languages to use
enable_language(C CXX)

file(GLOB SRCS "*.cpp" "utils/*.cpp")

set_source_files_properties(${SRCS} PROPERTIES COMPILE_FLAGS
                                               "-std=c++17 -fPIC -g -Wall")
set(PassName "SLAMP")

include_directories(./)

add_llvm_library(${PassName} SHARED ${SRCS})
