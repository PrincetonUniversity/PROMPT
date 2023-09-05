# Project
cmake_minimum_required(VERSION 3.4.3)
project(PROMPT)

# Find and link with LLVM
find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

# Find and set SCAF
if(DEFINED ENV{SCAF_INSTALL_DIR})
  set(SCAF_INSTALL_DIR $ENV{SCAF_INSTALL_DIR})
  message(STATUS "Found SCAF at ${SCAF_INSTALL_DIR}")
else()
  message(FATAL_ERROR "Cannot find SCAF, did you export $SCAF_INSTALL_DIR?")
endif()

# Find and set NOELLE
if(DEFINED ENV{NOELLE_INSTALL_DIR})
  set(NOELLE_INSTALL_DIR $ENV{NOELLE_INSTALL_DIR})
  message(STATUS "Found NOELLE at ${NOELLE_INSTALL_DIR}")
else()
  message(FATAL_ERROR "Cannot find NOELLE, did you export $NOELLE_INSTALL_DIR?")
endif()

# Option RUNTIME_LTO
option(RUNTIME_LTO "Enable runtime LTO" ON)
message(STATUS "RUNTIME_LTO: ${RUNTIME_LTO}")

# Option Comparison runtime
option(DO_COMPARE "Compare other runtimes (SLAMPboost)" OFF)
message(STATUS "DO_COMPARE: ${DO_COMPARE}")

add_subdirectory(src)
