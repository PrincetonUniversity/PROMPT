# Define the library target
add_library(malloc_hook SHARED malloc_hook.c)

# Set the library properties
target_include_directories(malloc_hook PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Install the library
install(TARGETS malloc_hook
    LIBRARY DESTINATION lib
    PUBLIC_HEADER DESTINATION include
)
