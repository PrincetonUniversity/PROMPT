set(SRCS
    "frontend.cpp"
    "slamp_ext_hooks.cpp"
)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -g")
set_source_files_properties(
  ${SRCS} PROPERTIES COMPILE_FLAGS "-Wno-inline -O3 -g -fexceptions"
)

# if RUNTIME_LTO is set, add LTO flags
if(RUNTIME_LTO)
  set_source_files_properties(${SRCS} PROPERTIES COMPILE_FLAGS "-flto")
endif()
set(PassName "slamp_hooks_custom")

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Boost REQUIRED COMPONENTS system)
include_directories(${Boost_INCLUDE_DIRS})

include_directories(./ ../)
add_library(${PassName} STATIC ${SRCS})
target_link_libraries(${PassName} ${Boost_LIBRARIES})

# install this
install(TARGETS ${PassName} DESTINATION ${CMAKE_INSTALL_PREFIX}/runtime)
