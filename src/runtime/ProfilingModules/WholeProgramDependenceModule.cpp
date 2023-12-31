#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <thread>
#include <unordered_set>
#include <vector>

#include "LoopHierarchy.h"
#include "MemoryProfile.h"
#include "Profile.h"
#include "WholeProgramDependenceModule.h"
#include "parallel_hashmap/phmap.h"
#include "slamp_shadow_mem.h"
#include "slamp_timestamp.h"

#define SIZE_8M 0x800000

namespace Profiling {
std::ostream &operator<<(std::ostream &stream, const MemoryProfile &vp) {
  stream << vp.total_count << " " << vp.loop_count << " ";
  return stream;
}

bool operator<(const ls_key_t &ls1, const ls_key_t &ls2) {
  return *((uint32_t *)&ls1) < *((uint32_t *)&ls2);
}
} // namespace Profiling

std::ostream &operator<<(std::ostream &stream, const Dependence &dep) {
  stream << dep.store << " " << dep.loop << " " << dep.dist << " " << dep.load;
  return stream;
}

void WholeProgramDependenceModule::initializeSets() {
  LoopInfoType &loopInfo = loop_hierarchy->getCurrentLoop();
  DependenceSets &dependenceSets = loopInfo.getItem();

  if (dependenceSets.size() != MemoryProfilerType::MAX_TRACKED_DISTANCE) {
    for (uint32_t i = dependenceSets.size();
         i < MemoryProfilerType::MAX_TRACKED_DISTANCE; i++) {
      DependenceSet seta;
      dependenceSets.push_back(seta);
    }
  }

  auto viter = dependenceSets.begin();
  for (; viter != dependenceSets.end(); viter++) {
    viter->clear();
  }
}

LoopInfoType &
WholeProgramDependenceModule::fillInDependence(const timestamp_t value,
                                               Dependence &dep) {
  const uint64_t store_time_stamp = value.timestamp;
  dep.store = value.instr;
  LoopInfoType &loop = loop_hierarchy->findLoop(store_time_stamp);
  dep.loop = loop.loop_id;
  dep.dist = loop_hierarchy->calculateDistance(loop, store_time_stamp);

  return loop;
}

// init: setup the shadow memory
void WholeProgramDependenceModule::init(uint32_t max_inst, uint32_t pid) {
  time_stamp = 1;

  lamp_stats.start_time = clock();

  loop_hierarchy = new Loops();
  memoryProfiler = new MemoryProfilerType(max_inst + 1);

  loop_hierarchy->loopIteration(0);

  LoopInfoType &loopInfo = loop_hierarchy->getCurrentLoop();
  DependenceSets dependenceSets(MemoryProfilerType::MAX_TRACKED_DISTANCE);
  loopInfo.setItem(dependenceSets);

  smmap->init_stack(SIZE_8M, pid);
}

// static phmap::flat_hash_map<uint32_t, uint32_t> store_inst_count;

void WholeProgramDependenceModule::fini(const char *filename) {

  // print sorted store_inst_count
  // std::map<uint32_t, uint32_t>
  // sorted_store_inst_count(store_inst_count.begin(),
  //                                                      store_inst_count.end());

  // for (auto &it : sorted_store_inst_count) {
  //   std::cout << it.first << " " << it.second << std::endl;
  // }

  auto lamp_out = new ofstream(filename);

  // Print out Loop Iteration Counts, sorted by the loop id
  std::map<uint32_t, uint32_t> sorted_loop_iteration_count(
      loop_iteration_count.begin(), loop_iteration_count.end());

  for (auto &it : sorted_loop_iteration_count) {
    *lamp_out << it.first << " " << it.second << "\n";
  }

  // Print out all dependence information
  *lamp_out << *memoryProfiler;

  // Print final stats
  *lamp_out << std::setprecision(3);
  *lamp_out << "run_time: "
            << 1.0 * (clock() - lamp_stats.start_time) / CLOCKS_PER_SEC << endl;
  *lamp_out << "Num dynamic stores: " << lamp_stats.dyn_stores << endl;
  *lamp_out << "Num dynamic loads: " << lamp_stats.dyn_loads << endl;
  *lamp_out << "Max loop nest depth: " << loop_hierarchy->max_depth << endl;
}

void WholeProgramDependenceModule::allocate(void *addr, uint64_t size) {
  smmap->allocate(addr, size);
  // clean up the page
  TS *s = (TS *)GET_SHADOW(addr, DM_TIMESTAMP_SIZE_IN_BYTES_LOG2);
  // LAMP doesn't actually invalidate anything,
  // Howver, this invalidation is necessary
  // FIXME: realloc is not handled here, realloc implies shadow mem to be copied
  // FIXME: only memset if it belongs to me
  // memset(s, 0, size * DM_TIMESTAMP_SIZE_IN_BYTES);
}

void WholeProgramDependenceModule::log(const timestamp_t ts,
                                       const uint32_t dst_inst) {
  Dependence dep(dst_inst);
  LoopInfoType &loopInfo = fillInDependence(ts, dep);
  dep.dist = MemoryProfilerType::trackedDistance(dep.dist);
  MemoryProfile &profile = memoryProfiler->increment(dep);

  DependenceSets &dependenceSets = loopInfo.getItem();
  pair<DependenceSet::iterator, bool> result =
      dependenceSets[dep.dist].insert(dep);
  if (result.second) {
    profile.incrementLoop();
  }
}

void WholeProgramDependenceModule::load(uint32_t instr, const uint64_t addr,
                                        const uint32_t bare_instr,
                                        const uint32_t size) {
  local_write(addr, [&]() {
    lamp_stats.dyn_loads++;
    TS *s = (TS *)GET_SHADOW(addr, DM_TIMESTAMP_SIZE_IN_BYTES_LOG2);

    timestamp_ts_u ts;
    TS last_ts = 0;

    for (uint32_t i = 0; i < size; i++) {
      if (s[i] != last_ts) {
        ts.ts = s[i];
        log(ts.timestamp, instr);
        last_ts = s[i];
      }
    }
  });
}

void WholeProgramDependenceModule::store(uint32_t instr, uint32_t bare_instr,
                                         const uint64_t addr,
                                         const uint32_t size) {
  local_write(addr, [&]() {
    // store_count++;
    lamp_stats.dyn_stores++;
    // if (lamp_stats.dyn_stores % 1'000 == 0) {
    // if (lamp_stats.dyn_stores > 54'000'000 &&
    //     lamp_stats.dyn_stores < 54'456'000) {
    //   std::cout << lamp_stats.dyn_stores << " " << instr << std::endl;
    // }

    // if (store_inst_count.find(instr) == store_inst_count.end())
    //   store_inst_count[instr] = 1;
    // else
    //   store_inst_count[instr]++;

    TS *shadow_addr = (TS *)GET_SHADOW(addr, DM_TIMESTAMP_SIZE_IN_BYTES_LOG2);

    timestamp_ts_u ts;
    ts.timestamp.instr = instr;
    ts.timestamp.timestamp = time_stamp;

    for (uint32_t i = 0; i < size; i++) {
      shadow_addr[i] = ts.ts;
    }
  });
}

void WholeProgramDependenceModule::loop_entry(uint32_t loop_id) {
  loop_hierarchy->enterLoop(loop_id, time_stamp);
  initializeSets();

  if (loop_iteration_count.find(loop_id) == loop_iteration_count.end())
    loop_iteration_count[loop_id] = 0;

  loop_iter();
}

void WholeProgramDependenceModule::loop_iter() {
  time_stamp++;
  loop_hierarchy->loopIteration(time_stamp);
  loop_iteration_count[loop_hierarchy->getCurrentLoop().loop_id]++;
  initializeSets();
}

void WholeProgramDependenceModule::loop_exit(uint32_t loop_id) {
  loop_hierarchy->exitLoop(loop_id);
}

void WholeProgramDependenceModule::merge_dep(
    WholeProgramDependenceModule &other) {
  // merge lamp_stats
  lamp_stats.dyn_loads += other.lamp_stats.dyn_loads;
  lamp_stats.dyn_stores += other.lamp_stats.dyn_stores;

  // merge memoryProfiler
  memoryProfiler->merge(*other.memoryProfiler);
}
