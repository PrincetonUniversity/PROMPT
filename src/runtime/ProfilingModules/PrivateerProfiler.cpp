#include "PrivateerProfiler.h"
#include "privateer/profiler.h"

// FIXME: a hack to hold the names
static const char *inst_names[500000];
static const char *fn_names[500000];
static const char *loop_names[500000];

void PrivateerProfiler::init(uint32_t loop_id, uint32_t pid) {
  Profiler &prof = Profiler::getInstance();
  prof.begin();

  // allocate chunks
  auto inst_name_chunk = (char *)malloc(500000 * 16);
  auto fn_name_chunk = (char *)malloc(500000 * 16);
  auto loop_name_chunk = (char *)malloc(500000 * 16);
  for (auto i = 0; i < 500000; i++) {
    // create a new string "inst_{i}" and store it in inst_names[i]
    auto inst_name = inst_name_chunk + (i * 16);
    auto fn_name = fn_name_chunk + (i * 16);
    auto loop_name = loop_name_chunk + (i * 16);
    sprintf(inst_name, "inst_%d", i);
    sprintf(fn_name, "fn_%d", i);
    sprintf(loop_name, "loop_%d", i);

    inst_names[i] = inst_name;
    fn_names[i] = fn_name;
    loop_names[i] = loop_name;
  }
}

void PrivateerProfiler::fini(const char *filename) {
  Profiler &prof = Profiler::getInstance();
  prof.end();
}

void PrivateerProfiler::allocate(void *addr, uint32_t instr, uint64_t size) {
  Profiler &prof = Profiler::getInstance();
  prof.malloc(inst_names[instr], addr, size);
}

void PrivateerProfiler::realloc(void *old_addr, void *new_addr, uint32_t instr,
                                uint64_t size) {
  Profiler &prof = Profiler::getInstance();
  prof.realloc(inst_names[instr], old_addr, new_addr, size);
}

void PrivateerProfiler::free(void *addr) {
  Profiler &prof = Profiler::getInstance();
  prof.free(nullptr, addr, false);
}

void PrivateerProfiler::stack_alloc(void *addr, uint32_t instr, uint64_t size) {
  Profiler &prof = Profiler::getInstance();
  prof.report_stack(inst_names[instr], addr, 1, size);
}

void PrivateerProfiler::stack_free(void *addr) {
  Profiler &prof = Profiler::getInstance();
  prof.free(nullptr, addr, true);
}

void PrivateerProfiler::func_entry(uint32_t fcnId) {
  Profiler &prof = Profiler::getInstance();
  prof.begin_function(fn_names[fcnId]);
}
void PrivateerProfiler::func_exit(uint32_t fcnId) {
  Profiler &prof = Profiler::getInstance();
  prof.end_function(0);
  // prof.end_function(fn_names[fcnId]);
}

void PrivateerProfiler::loop_entry(uint32_t loopId) {
  Profiler &prof = Profiler::getInstance();
  prof.begin_iter(loop_names[loopId]);
}

void PrivateerProfiler::loop_exit(uint32_t loopId) {
  Profiler &prof = Profiler::getInstance();
  prof.end_iter(0);
  // prof.end_iter(loop_names[loopId]);
}

void PrivateerProfiler::loop_iter(uint32_t loopId) {
  Profiler &prof = Profiler::getInstance();
  // FIXME: this is a hack to get around the fact that we don't have a
  // loop_iter_end
  prof.end_iter(0);
  prof.begin_iter(loop_names[loopId]);
  // prof.end_iter(loop_names[loopId]);
  // prof.begin_iter(loop_names[loopId]);
}

void PrivateerProfiler::points_to_inst(uint32_t instId, void *ptr) {
  Profiler &prof = Profiler::getInstance();

  prof.find_underlying_object(inst_names[instId], ptr);
  prof.predict_ptr(inst_names[instId], ptr);
}

void PrivateerProfiler::points_to_arg(uint32_t fcnId, uint32_t argId,
                                      void *ptr) {
  // FIXME: implement this
}

void PrivateerProfiler::load(uint32_t instr, uint64_t value) {
  Profiler &prof = Profiler::getInstance();
  prof.predict_int(inst_names[instr], value);
}

void PrivateerProfiler::predict_int(uint32_t instId, uint64_t value) {}

void PrivateerProfiler::predict_ptr(uint32_t instId, void *ptr) {}