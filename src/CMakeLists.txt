# Project
cmake_minimum_required(VERSION 3.4.3)
project(PROMPT)

# Find and link with LLVM
find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

# Option RUNTIME_LTO
option(RUNTIME_LTO "Enable runtime LTO" ON)
message(STATUS "RUNTIME_LTO: ${RUNTIME_LTO}")

# Option Comparison runtime
option(DO_COMPARE "Compare other runtimes (SLAMPboost)" OFF)
message(STATUS "DO_COMPARE: ${DO_COMPARE}")

add_subdirectory(lib)
add_subdirectory(runtime)

