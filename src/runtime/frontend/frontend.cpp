#include "slamp_hooks.h"
#include "slamp_produce.h"
#include <cstdint>
#include <cstdio>
#include <iostream>

#include "malloc_hook/malloc_hook.h"

extern "C" bool hook_enabled;

// #define SAMPLING_ITER
static int nested_level = 0;
static bool on_profiling = false;

static uint32_t ext_fn_inst_id = 0;

static void *(*old_malloc_hook)(size_t, const void *);
static void *(*old_realloc_hook)(void *, size_t, const void *);
static void (*old_free_hook)(void *, const void *);
static void *(*old_memalign_hook)(size_t, size_t, const void *);

#ifndef PRODUCE_QUEUE_DEFINE
#define PRODUCE_QUEUE_DEFINE()
#endif

#ifndef PRODUCE_QUEUE_INIT
#define PRODUCE_QUEUE_INIT()
#endif

#ifndef PRODUCE_QUEUE_FLUSH
#define PRODUCE_QUEUE_FLUSH()
#endif

#ifndef PRODUCE_INIT
#define PRODUCE_INIT(loop_id, pid)
#endif

#ifndef PRODUCE_ALLOC
#define PRODUCE_ALLOC(inst_id, size, addr)
#endif

#ifndef PRODUCE_FREE
#define PRODUCE_FREE(addr)
#endif

#ifndef PRODUCE_FINISHED
#define PRODUCE_FINISHED()
#endif

#ifndef PRODUCE_FUNC_ENTRY
#define PRODUCE_FUNC_ENTRY(fn_id)
#endif

#ifndef PRODUCE_FUNC_EXIT
#define PRODUCE_FUNC_EXIT(fn_id)
#endif

#ifndef PRODUCE_LOOP_ENTRY
#define PRODUCE_LOOP_ENTRY(loop_id)
#endif

#ifndef PRODUCE_LOOP_EXIT
#define PRODUCE_LOOP_EXIT(loop_id)
#endif

#ifndef PRODUCE_LOOP_ITER_CTX
#define PRODUCE_LOOP_ITER_CTX(loop_id)
#endif

#ifndef PRODUCE_LOOP_INVOC
#define PRODUCE_LOOP_INVOC()
#endif

#ifndef PRODUCE_LOOP_ITER
#define PRODUCE_LOOP_ITER()
#endif

#ifndef PRODUCE_POINTS_TO_ARG
#define PRODUCE_POINTS_TO_ARG(fn_arg_id, addr)
#endif

#ifndef PRODUCE_POINTS_TO_INST
#define PRODUCE_POINTS_TO_INST(inst_id, addr)
#endif

#ifndef PRODUCE_LOAD
#define PRODUCE_LOAD(instr, addr, value)
#endif

#ifndef PRODUCE_STORE
#define PRODUCE_STORE(instr, addr)
#endif

static volatile char *lc_dummy = NULL;

PRODUCE_QUEUE_DEFINE();

void SLAMP_init(uint32_t fn_id, uint32_t loop_id) {

  PRODUCE_QUEUE_INIT();
  uint32_t pid = getpid();

  PRODUCE_INIT(loop_id, pid);

  auto allocateLibcReqs = [](void *addr, size_t size) {
    PRODUCE_ALLOC(0, size, (uint64_t)addr);
    // produce_8_24_32_64(ALLOC, 0, size, (uint64_t)addr);
  };

  allocateLibcReqs((void *)&errno, sizeof(errno));
  allocateLibcReqs((void *)&stdin, sizeof(stdin));
  allocateLibcReqs((void *)&stdout, sizeof(stdout));
  allocateLibcReqs((void *)&stderr, sizeof(stderr));
  allocateLibcReqs((void *)&sys_nerr, sizeof(sys_nerr));

  // FIXME: a dirty way to get locale updated
  lc_dummy = setlocale(LC_ALL, "");

  // TURN ON HOOKS
  hook_enabled = true;

  // whole program profiling
  if (loop_id == 0) {
    on_profiling = true;
  }

  // flush
  PRODUCE_QUEUE_FLUSH_AND_WAIT();
}

void SLAMP_fini(const char *filename) {
  PRODUCE_FINISHED();

  PRODUCE_QUEUE_FLUSH();

  // TURN OFF HOOKS
  hook_enabled = false;

  if (nested_level != 0) {
    std::cerr << "Error: nested_level != 0 on exit" << std::endl;
    exit(-1);
  }
}

void SLAMP_init_global_vars(const char *name, uint64_t addr, size_t size) {
  PRODUCE_ALLOC(0, size, addr);
}

void SLAMP_enter_fcn(uint32_t id) { PRODUCE_FUNC_ENTRY(id); }

void SLAMP_exit_fcn(uint32_t id) { PRODUCE_FUNC_EXIT(id); }

void SLAMP_enter_loop(uint32_t id) { PRODUCE_LOOP_ENTRY(id); }

void SLAMP_exit_loop(uint32_t id) { PRODUCE_LOOP_EXIT(id); }

void SLAMP_loop_iter_ctx(uint32_t id) {
  PRODUCE_LOOP_ITER_CTX();
  // produce_32_32(LOOP_ITER_CTX, id);
}

void SLAMP_loop_invocation() {
  PRODUCE_LOOP_INVOC();

  nested_level++;
  on_profiling = true;
}

void SLAMP_loop_iteration() {
  PRODUCE_LOOP_ITER();

#ifdef SAMPLING_ITER
  if (counter_iter % 100 == 0) {
    on_profiling = true;
  }
  if (counter_iter % 100 == 10) { // 0-10000 out of 100000, sampling 10%
    on_profiling = false;
  }
  counter_iter++;
#endif
}

void SLAMP_loop_exit() {
  nested_level--;
  if (nested_level < 0) {
    // huge problem
    std::cerr << "Error: nested_level < 0" << std::endl;
    exit(-1);
  }
  if (nested_level == 0) {
    on_profiling = false;
  }
}

void SLAMP_report_base_pointer_arg(uint32_t fcnId, uint32_t argId, void *ptr) {
  // FIXME: combine fcnid and argid to 32 bit
  uint32_t fn_arg_id = (fcnId << 16) | (argId & 0xffff);
  PRODUCE_POINTS_TO_ARG(fn_arg_id, (uint64_t)ptr);
}

void SLAMP_report_base_pointer_inst(uint32_t instId, void *ptr) {
  PRODUCE_POINTS_TO_INST(instId, (uint64_t)ptr);
}

// TODO: this should be optional
void SLAMP_ext_push(const uint32_t instr) ATTRIBUTE(always_inline) {
  ext_fn_inst_id = instr;
}

void SLAMP_ext_pop() ATTRIBUTE(always_inline) { ext_fn_inst_id = 0; }

void SLAMP_load(const uint32_t instr, const uint64_t addr,
                const uint32_t bare_instr, uint64_t value)
    ATTRIBUTE(always_inline) {
  if (on_profiling) {
    PRODUCE_LOAD(instr, addr, value);
  }
}

void SLAMP_load1(uint32_t instr, const uint64_t addr, const uint32_t bare_instr,
                 uint64_t value) {
  SLAMP_load(instr, addr, bare_instr, value);
}
void SLAMP_load2(uint32_t instr, const uint64_t addr, const uint32_t bare_instr,
                 uint64_t value) {
  SLAMP_load(instr, addr, bare_instr, value);
}
void SLAMP_load4(uint32_t instr, const uint64_t addr, const uint32_t bare_instr,
                 uint64_t value) {
  SLAMP_load(instr, addr, bare_instr, value);
}

void SLAMP_load8(uint32_t instr, const uint64_t addr, const uint32_t bare_instr,
                 uint64_t value) {
  SLAMP_load(instr, addr, bare_instr, value);
}

void SLAMP_loadn(uint32_t instr, const uint64_t addr, const uint32_t bare_instr,
                 size_t n) {
  SLAMP_load(instr, addr, bare_instr, 0);
}

void SLAMP_load1_ext(const uint64_t addr, const uint32_t bare_instr,
                     uint64_t value) {
  SLAMP_load1(bare_instr, addr, bare_instr, value);
}
void SLAMP_load2_ext(const uint64_t addr, const uint32_t bare_instr,
                     uint64_t value) {
  SLAMP_load2(bare_instr, addr, bare_instr, value);
}
void SLAMP_load4_ext(const uint64_t addr, const uint32_t bare_instr,
                     uint64_t value) {
  SLAMP_load4(bare_instr, addr, bare_instr, value);
}
void SLAMP_load8_ext(const uint64_t addr, const uint32_t bare_instr,
                     uint64_t value) {
  SLAMP_load8(bare_instr, addr, bare_instr, value);
}
void SLAMP_loadn_ext(const uint64_t addr, const uint32_t bare_instr, size_t n) {
  SLAMP_loadn(bare_instr, addr, bare_instr, n);
}

void SLAMP_store(const uint32_t instr, const uint64_t addr,
                 const uint32_t bare_instr) ATTRIBUTE(always_inline) {
  if (on_profiling) {
    PRODUCE_STORE(instr, addr);
  }
}

void SLAMP_store1(uint32_t instr, const uint64_t addr) {
  SLAMP_store(instr, addr, instr);
}

void SLAMP_store2(uint32_t instr, const uint64_t addr) {
  SLAMP_store(instr, addr, instr);
}
void SLAMP_store4(uint32_t instr, const uint64_t addr) {
  SLAMP_store(instr, addr, instr);
}
void SLAMP_store8(uint32_t instr, const uint64_t addr) {
  SLAMP_store(instr, addr, instr);
}
void SLAMP_storen(uint32_t instr, const uint64_t addr, size_t n) {
  SLAMP_store(instr, addr, instr);
}

void SLAMP_store1_ext(const uint64_t addr, const uint32_t bare_inst) {
  SLAMP_store1(bare_inst, addr);
}
void SLAMP_store2_ext(const uint64_t addr, const uint32_t bare_inst) {
  SLAMP_store2(bare_inst, addr);
}
void SLAMP_store4_ext(const uint64_t addr, const uint32_t bare_inst) {
  SLAMP_store4(bare_inst, addr);
}
void SLAMP_store8_ext(const uint64_t addr, const uint32_t bare_inst) {
  SLAMP_store8(bare_inst, addr);
}
void SLAMP_storen_ext(const uint64_t addr, const uint32_t bare_inst, size_t n) {
  SLAMP_storen(bare_inst, addr, n);
}

void malloc_callback(void *ptr, size_t size) {
  PRODUCE_ALLOC(ext_fn_inst_id, size, (uint64_t)ptr);
}

void calloc_callback(void *ptr, size_t nmemb, size_t size) {
  PRODUCE_ALLOC(ext_fn_inst_id, size * nmemb, (uint64_t)ptr);
}

void free_callback(void *ptr) { PRODUCE_FREE((uint64_t)ptr); }

void realloc_callback(void *new_ptr, void *ptr, size_t size) {
  // TODO: the old pointer might be freed. Need to check whether the two
  // pointers are the same
  PRODUCE_ALLOC(ext_fn_inst_id, size, (uint64_t)new_ptr);
}
void memalign_callback(void *ptr, size_t alignment, size_t size) {
  PRODUCE_ALLOC(ext_fn_inst_id, size, (uint64_t)ptr);
}

// FIXME: a bunch of unused functions
void SLAMP_main_entry(uint32_t argc, char **argv, char **env) {}
void SLAMP_push(const uint32_t instr) {}
void SLAMP_pop() {}
void SLAMP_allocated(uint64_t addr) {}
void SLAMP_callback_stack_alloca(uint64_t, uint64_t, uint32_t, uint64_t) {}
void SLAMP_callback_stack_free() {}
