set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Boost REQUIRED COMPONENTS system)
find_package(
  Python3
  COMPONENTS Interpreter
  REQUIRED)
message(STATUS "Python3 Executable: ${Python3_EXECUTABLE}")

set(SRCS "frontend.cpp" "slamp_ext_hooks.cpp")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -g")
set_source_files_properties(
  ${SRCS} PROPERTIES COMPILE_FLAGS "-Wno-inline -O3 -g -fexceptions")

if(RUNTIME_LTO)
  set_source_files_properties(${SRCS} PROPERTIES COMPILE_FLAGS "-flto")
endif()

# Generate headers for each configuration
set(CONFIGS
    "wp-dep"
    "dep"
    "dep-context"
    "ol"
    "pt"
    "lv"
    "privateer")
foreach(CONFIG ${CONFIGS})
  set(CONFIG_BUILD_DIR ${CMAKE_BINARY_DIR}/${CONFIG})
  set(HEADER_PATH ${CONFIG_BUILD_DIR}/slamp_produce.h)

  add_custom_command(
    OUTPUT ${HEADER_PATH}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CONFIG_BUILD_DIR}
    COMMAND
      ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/FrontendGenerator.py -m
      ${CONFIG} --config-dir ${CMAKE_CURRENT_SOURCE_DIR}/../Events/configs
      --template ${CMAKE_CURRENT_SOURCE_DIR}/custom_produce.h --output
      ${HEADER_PATH}
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/FrontendGenerator.py
    COMMENT "Generating slamp_produce.h for configuration ${CONFIG}")

  set(HEADER_FILES ${HEADER_FILES} ${HEADER_PATH})
endforeach()

set(PassName "slamp_hooks_custom")

include_directories(${Boost_INCLUDE_DIRS})
# for the headers and malloc_hook
include_directories(./ ../)

# generate a library for each configuration
foreach(CONFIG ${CONFIGS})
  set(PassConfig ${PassName}_${CONFIG})
  add_library(${PassConfig} STATIC
              ${SRCS} ${CMAKE_BINARY_DIR}/${CONFIG}/slamp_produce.h)
  target_include_directories(${PassConfig}
                             PRIVATE ${CMAKE_BINARY_DIR}/${CONFIG})
  target_link_libraries(${PassConfig} ${Boost_LIBRARIES})
  install(TARGETS ${PassConfig} DESTINATION ${CMAKE_INSTALL_PREFIX}/runtime)
endforeach()

set_directory_properties(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES
                                    "${HEADER_FILES}")
