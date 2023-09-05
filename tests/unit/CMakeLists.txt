enable_testing()

# Find and link with LLVM
find_package(LLVM REQUIRED CONFIG)


# set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -g -Weverything")
# set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -Weverything")
# set( CMAKE_EXPORT_COMPILE_COMMANDS ON )

# Link with LLVM
include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Prepare the pass to be included in the source tree
list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(AddLLVM)

# All include files for CPF
include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_BINARY_DIR}/include
  ${CMAKE_INSTALL_PREFIX}/include ${SCAF_INSTALL_DIR}/include
  ${NOELLE_INSTALL_DIR}/include ${NOELLE_INSTALL_DIR}/include/svf)

add_executable(CastUtilTest CastUtilTest.cpp)

llvm_map_components_to_libnames(llvm_libs support core irreader analysis TransformUtils)
# Link the test executable with the necessary libraries
target_link_libraries(
  CastUtilTest
  PRIVATE GTest::gtest_main
          SLAMP
          ${SCAF_INSTALL_DIR}/lib/libSCAFUtilities.so
          ${llvm_libs}
          )

# Manually specify the include directory for the test target
target_include_directories(CastUtilTest
                           PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../src/lib)

include(GoogleTest)
gtest_discover_tests(CastUtilTest)
