# Project

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

#set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -g -Weverything")
#set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -Weverything")
#set( CMAKE_EXPORT_COMPILE_COMMANDS ON )

# Link with LLVM
include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Prepare the pass to be included in the source tree
list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(AddLLVM)

# All include files for CPF
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include
                    ${CMAKE_CURRENT_BINARY_DIR}/include
                    ${CMAKE_INSTALL_PREFIX}/include
                    ${SCAF_INSTALL_DIR}/include
                    ${NOELLE_INSTALL_DIR}/include
                    ${NOELLE_INSTALL_DIR}/include/svf)

# Programming languages to use
enable_language(C CXX)

file(GLOB SRCS
    "*.cpp"
)

# Compilation flags
add_compile_options(-DITO_ENABLE)

set_source_files_properties(${SRCS} PROPERTIES COMPILE_FLAGS "-std=c++17 -fPIC -g -Wall")
set(PassName "SLAMP")

list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
#include(HandleLLVMOptions)
include(AddLLVM)

include_directories(./)

#add_llvm_library(${PassName} MODULE ${SRCS})
add_llvm_library(${PassName} SHARED ${SRCS}) # This is to generate libxxx.so
