#include <cstdint>
#include <fstream>
#include <map>
#include <set>
#include <thread>
#include <unordered_set>
#include <vector>

#include "DependenceModule.h"
#include "slamp_logger.h"
#include "slamp_shadow_mem.h"
#include "slamp_timestamp.h"

// static std::map<uint32_t, uint64_t> *inst_count;
static inline uint64_t rdtsc() {
  uint64_t a, d;
  __asm__ volatile("rdtsc" : "=a"(a), "=d"(d));
  return (d << 32) | a;
}

// init: setup the shadow memory
void DependenceModule::init(uint32_t loop_id, uint32_t pid)
    __attribute__((always_inline)) {
  target_loop_id = loop_id;

#define SIZE_8M 0x800000
  smmap->init_stack(SIZE_8M, pid);
  // inst_count = new std::map<uint32_t, uint64_t>();
}

// static uint64_t log_time = 0;

void DependenceModule::fini(const char *filename) {
  // std::cout << "Load count: " << load_count << std::endl;
  // std::cout << "Store count: " << store_count << std::endl;

  std::ofstream of(filename);
  of << target_loop_id << " " << 0 << " " << 0 << " " << 0 << " " << 0 << " "
     << 0 << "\n";

#ifdef TRACK_COUNT
  // get all the keys of a hash table to ordered set
  std::set<slamp::KEY, slamp::KEYComp> ordered;
  for (auto &it : deps) {
    ordered.insert(it.first);
  }
#else
  std::set<slamp::KEY, slamp::KEYComp> ordered(deps.begin(), deps.end());
#endif

  for (auto &k : ordered) {
#ifdef TRACK_COUNT
    auto count = deps[k];
#else
    auto count = 1;
#endif
    of << target_loop_id << " " << k.src << " " << k.dst << " " << k.dst_bare
       << " " << (k.cross ? 1 : 0) << " " << count << " ";
#ifdef TRACK_MIN_DISTANCE
    auto dist = min_dist[k];
    of << dist;
#endif
    of << "\n";
  }
  of.close();

  // std::cout << "Log time: " << log_time/ 2.6e9 << " s" << std::endl;

  // for (auto &i : *inst_count) {
  //   of << target_loop_id << " " << i.first << " " << i.second << "\n";
  // }

#ifdef COLLECT_TRACE
  // dump the dep trace to a binary file
  std::ofstream trace_file("dep_trace.bin", std::ios::binary);
  trace_file.write((char *)dep_trace.data(),
                   dep_trace.size() * sizeof(slamp::KEY));
  trace_file.close();
#endif
}

void DependenceModule::allocate(void *addr, uint64_t size)
    __attribute__((always_inline)) {
  smmap->allocate(addr, size);
  // FIXME: might need to clean up the page!
}

void DependenceModule::log(TS ts, const uint32_t dst_inst,
                           const uint32_t context) {

  uint32_t src_inst = GET_INSTR(ts);

  uint64_t src_invoc = GET_INVOC(ts);
  uint64_t src_iter = GET_ITER(ts);

  if (src_invoc != GET_INVOC(slamp_invocation)) {
    return;
  }

  slamp::KEY key(src_inst, dst_inst, context, src_iter != slamp_iteration);

#ifdef TRACK_MIN_DISTANCE
  auto dist = slamp_iteration - src_iter;
  min_dist.emplace({key, dist});
#endif

  deps.emplace(key);

#ifdef COLLECT_TRACE
  if (dep_trace_idx < dep_trace_size) {
    dep_trace.emplace_back(key);
    dep_trace_idx++;
  }
#endif
}

void DependenceModule::load(uint32_t instr, const uint64_t addr,
                            const uint32_t bare_instr)
    __attribute__((always_inline)) {
  local_write(addr, [&]() {
    // load_count++;

    // if tracking multiple loops

    TS *s = (TS *)GET_SHADOW(addr, DM_TIMESTAMP_SIZE_IN_BYTES_LOG2);

    TS tss = s[0];
    if (tss != 0) {
      // uint64_t start = rdtsc();
      log(tss, instr, context);
      // uint64_t end = rdtsc();
      // log_time += end - start;
    }
#ifdef TRACK_WAR
    if (context != 0) {
      instr = context;
    }
    TS ts = CREATE_TS(instr, slamp_iteration, slamp_invocation);
    s[1] = ts;
#endif
  });
}

void DependenceModule::store(uint32_t instr, uint32_t bare_instr,
                             const uint64_t addr)
    __attribute__((always_inline)) {

  local_write(addr, [&]() {
    // store_count++;
    TS *shadow_addr = (TS *)GET_SHADOW(addr, DM_TIMESTAMP_SIZE_IN_BYTES_LOG2);

#ifdef TRACK_WAW
    if (shadow_addr[0] != 0) {
      // uint64_t start = rdtsc();
      log(shadow_addr[0], instr, context);
      // uint64_t end = rdtsc();
      // log_time += end - start;
    }
#endif

#ifdef TRACK_WAR
    if (shadow_addr[1] != 0) {
      // uint64_t start = rdtsc();
      log(shadow_addr[1], instr, context);
      // uint64_t end = rdtsc();
      // log_time += end - start;
    }
#endif

    if (context != 0) {
      instr = context;
    }
    TS ts = CREATE_TS(instr, slamp_iteration, slamp_invocation);
    shadow_addr[0] = ts;
  });
}

void DependenceModule::loop_invoc() __attribute__((always_inline)) {
  slamp_iteration = 0;
  slamp_invocation++;
  nested_level++;
}

void DependenceModule::loop_iter() __attribute__((always_inline)) {

  slamp_iteration++;
}

void DependenceModule::loop_exit() __attribute__((always_inline)) {
  nested_level--;
}

void DependenceModule::func_entry(uint32_t instr) {
  if (nested_level == 1) {
    context = instr;
  }
}

void DependenceModule::func_exit(uint32_t instr) {
  if (nested_level == 1) {
    context = 0;
  }
}

void DependenceModule::merge_dep(DependenceModule &other) {
  deps.merge(other.deps);
#ifdef TRACK_MIN_DISTANCE
  min_dist.merge(other.min_dist);
#endif
}
