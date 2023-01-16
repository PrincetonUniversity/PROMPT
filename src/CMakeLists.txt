# Project
cmake_minimum_required(VERSION 3.4.3)
project(PROMPT)

# Find and link with LLVM
find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

add_subdirectory(lib)
add_subdirectory(runtime)

