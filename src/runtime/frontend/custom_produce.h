#pragma once

#include "../SLAMPcustom/sw_queue_astream.h"
#include <boost/interprocess/managed_shared_memory.hpp>
namespace bip = boost::interprocess;

enum UnifiedAction : char {
  INIT = 0,
  LOAD,
  STORE,
  ALLOC,
  REALLOC,
  FREE,
  STACK_LIFETIME_START,
  STACK_LIFETIME_END,
  TARGET_LOOP_INVOC,
  TARGET_LOOP_ITER,
  TARGET_LOOP_EXIT,
  LOOP_ENTRY,
  LOOP_EXIT,
  LOOP_ITER_CTX,
  FUNC_ENTRY,
  FUNC_EXIT,
  POINTS_TO_INST,
  POINTS_TO_ARG,
  FUNC_CALL_PUSH,
  FUNC_CALL_POP,
  FINISHED
};

#define PRODUCE_QUEUE_INIT()                                                   \
  do {                                                                         \
    char *env = getenv("SLAMP_QUEUE_ID");                                      \
    if (env == NULL) {                                                         \
      std::cerr << "SLAMP_QUEUE_ID not set" << std::endl;                      \
      exit(-1);                                                                \
    }                                                                          \
    auto queue_name = std::string("slamp_queue_") + env;                       \
    auto segment = new bip::fixed_managed_shared_memory(                       \
        bip::open_or_create, queue_name.c_str(), sizeof(uint32_t) * QSIZE * 4, \
        (void *)(1UL << 32));                                                  \
    Queue_p dqA, dqB;                                                          \
    dqA = segment->find<Queue>("DQ_A").first;                                  \
    dqB = segment->find<Queue>("DQ_B").first;                                  \
    init(dqA, dqB);                                                            \
  } while (0)

#define PRODUCE_QUEUE_FLUSH() flush();
#define PRODUCE_QUEUE_FLUSH_AND_WAIT() produce_wait();

/// Additional macros
