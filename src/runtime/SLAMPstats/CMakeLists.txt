file(GLOB SRCS
    "*.cpp"
)

# Compilation flags
# set_source_files_properties(${SRCS} PROPERTIES COMPILE_FLAGS "-Wl,-save-temps -std=c++17 -Wno-inline -O3 -fexceptions")# -emit-llvm")
set_source_files_properties(${SRCS} PROPERTIES COMPILE_FLAGS "-std=c++17 -Wno-inline -O3 -fexceptions")# -emit-llvm")
# if RUNTIME_LTO is set, add LTO flags
if(RUNTIME_LTO)
    set_source_files_properties(${SRCS} PROPERTIES COMPILE_FLAGS "-flto")
endif()
set(PassName "slamp_hooks_stats")

# list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
#include(HandleLLVMOptions)
# include(AddLLVM)


add_llvm_library(${PassName} STATIC ${SRCS})

# install this
install(TARGETS ${PassName} DESTINATION ${CMAKE_INSTALL_PREFIX}/runtime)
