cmake_minimum_required (VERSION 3.6.2 FATAL_ERROR)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

find_package(Boost REQUIRED COMPONENTS system)
include_directories(${Boost_INCLUDE_DIRS})

# if RUNTIME_LTO is set, enable LTO
if (RUNTIME_LTO)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -flto")
    set(LINK_FLAGS "${LINK_FLAGS} -flto")
endif()

add_executable (consumer_boost consumer.cpp ./ProfilingModules/DependenceModule.cpp)
target_link_libraries(consumer_boost LINK_PUBLIC
	${Boost_LIBRARIES}
	rt
	Threads::Threads
)

# install this
install(TARGETS consumer_boost DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
