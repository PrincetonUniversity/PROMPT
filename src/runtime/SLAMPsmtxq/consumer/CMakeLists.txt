cmake_minimum_required (VERSION 3.6.2 FATAL_ERROR)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# if RUNTIME_LTO is set, enable LTO
if (RUNTIME_LTO)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -flto")
    set(LINK_FLAGS "${LINK_FLAGS} -flto")
endif()

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
