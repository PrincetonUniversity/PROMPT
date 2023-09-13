# Project
cmake_minimum_required(VERSION 3.4.3)
project(PROMPT)

# Find and link with LLVM
find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

# Option RUNTIME_LTO
option(RUNTIME_LTO "Enable runtime LTO" ON)
option(DO_COMPARE "Compare other runtimes (SLAMPboost)" OFF)
option(DO_STATS "Collect statistics" OFF)


message(STATUS "RUNTIME_LTO: ${RUNTIME_LTO}")
message(STATUS "DO_COMPARE: ${DO_COMPARE}")
message(STATUS "DO_STATS: ${DO_STATS}")

# ignore warning Wgcc-compat
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-gcc-compat -Wno-unknown-attributes")

add_subdirectory(src)
