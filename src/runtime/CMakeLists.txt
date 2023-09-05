# frontend
add_subdirectory(ProfilingModules)
add_subdirectory(SLAMPcustom)
add_subdirectory(SLAMPstats)

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
