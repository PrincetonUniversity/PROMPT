#pragma once

#include "../SLAMPcustom/sw_queue_astream.h"
#include <boost/interprocess/managed_shared_memory.hpp>
namespace bip = boost::interprocess;

enum UnifiedAction : char {
  INIT = 0,
  LOAD,
  STORE,
  ALLOC,
  FREE,
  LOOP_INVOC,
  LOOP_ITER,
  LOOP_ENTRY,
  LOOP_EXIT,
  LOOP_ITER_CTX,
  FUNC_ENTRY,
  FUNC_EXIT,
  POINTS_TO_INST,
  POINTS_TO_ARG,
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
    auto segment = new bip::fixed_managed_shared_memory(                            \
        bip::open_or_create, queue_name.c_str(), sizeof(uint32_t) * QSIZE * 4, \
        (void *)(1UL << 32));                                                  \
    Queue_p dqA, dqB;                                                          \
    dqA = segment->find<Queue>("DQ_A").first;                                  \
    dqB = segment->find<Queue>("DQ_B").first;                                  \
    dq = new DoubleQueue_Producer(dqA, dqB);                                   \
  } while (0)

#define PRODUCE_QUEUE_FLUSH() dq->flush();
#define PRODUCE_QUEUE_DEFINE() DoubleQueue_Producer *dq

/// Additional macros
#define PRODUCE_INIT(func_id, loop_id) dq->produce_8_32_32(INIT, func_id, loop_id)
#define PRODUCE_LOAD(instr, addr, value) dq->produce_8_32_64(LOAD, instr, addr)
#define PRODUCE_STORE(instr, addr) dq->produce_8_32_64(STORE, instr, addr)
#define PRODUCE_ALLOC(inst_id, size, ptr) dq->produce_8_32_64(ALLOC, size, ptr)
#define PRODUCE_LOOP_INVOC() dq->produce_8(LOOP_INVOC)
#define PRODUCE_LOOP_ITER() dq->produce_8(LOOP_ITER)
#define PRODUCE_FUNC_ENTRY(function_id) dq->produce_8_32(FUNC_ENTRY, function_id)
#define PRODUCE_FUNC_EXIT(function_id) dq->produce_8_32(FUNC_EXIT, function_id)
#define PRODUCE_FINISHED() dq->produce_8(FINISHED)