# Add third-party libraries
add_subdirectory(third_party/cxxopts)

# malloc hook
add_subdirectory(malloc_hook)

# frontend
add_subdirectory(ProfilingModules)
add_subdirectory(frontend)

#if(DO_STATS)
  add_subdirectory(SLAMPstats)
  #endif()

if(DO_COMPARE)
  add_subdirectory(SLAMPboost)
  add_subdirectory(SLAMPsmtxq)
endif()

# backend
add_subdirectory(SLAMPcustom/consumer)

if(DO_COMPARE)
  add_subdirectory(SLAMPboost/consumer)
  add_subdirectory(SLAMPsmtxq/consumer)
endif()
