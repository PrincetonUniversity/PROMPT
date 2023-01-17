cmake_minimum_required (VERSION 3.6.2 FATAL_ERROR)

set(CMAKE_C_COMPILER "clang")
set(CMAKE_CXX_COMPILER "clang++")
set(CMAKE_LINKER "ld.gold")

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -flto -Ofast")
set(LINK_FLAGS "${LINK_FLAGS} -flto -O3")

find_package(Boost REQUIRED COMPONENTS system)
include_directories(${Boost_INCLUDE_DIRS})
include_directories(./ ../ ../../)

add_executable (consumer_smtx consumer.cpp ../sw_queue.c)
target_link_libraries(consumer_smtx LINK_PUBLIC 
	${Boost_LIBRARIES}
    ProfilingModules
	rt
    Threads::Threads)

# install this
install(TARGETS consumer_smtx DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
