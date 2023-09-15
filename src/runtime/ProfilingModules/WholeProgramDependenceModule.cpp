#include <cstdint>
#include <fstream>
#include <map>
#include <set>
#include <thread>
#include <unordered_set>
#include <vector>
#include <iomanip>

#include "WholeProgramDependenceModule.h"
#include "slamp_logger.h"
#include "slamp_shadow_mem.h"
#include "slamp_timestamp.h"

#define SIZE_8M  0x800000

uint32_t giNumLoops;

struct timestamp_s {
  uint32_t instr : 20;
  uint64_t timestamp : 44;
} __attribute__((__packed__));

using timestamp_t = struct timestamp_s;

union timestamp_ts_u {
  timestamp_t timestamp;
  TS ts;
};

using lamp_stats_t = struct _lamp_stats_t {
  clock_t start_time;
  int64_t dyn_stores, dyn_loads, num_sync_arcs;
  int64_t calls_to_qhash;
  uint32_t nest_depth;
};

using MemoryProfilerType = MemoryProfiler<MAX_DEP_DIST>;

static lamp_stats_t lamp_stats;

static MemoryProfilerType *memoryProfiler;
static Loops *loop_hierarchy;
uint64_t *iterationcount;

static uint64_t time_stamp;

static void initializeSets() {
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

static LoopInfoType &fillInDependence(const timestamp_t value,
                                      Dependence &dep) {
  const uint64_t store_time_stamp = value.timestamp;
  dep.store = value.instr;
  LoopInfoType &loop = loop_hierarchy->findLoop(store_time_stamp);
  dep.loop = loop.loop_id;
  dep.dist = loop_hierarchy->calculateDistance(loop, store_time_stamp);

  return loop;
}

// init: setup the shadow memory
void WholeProgramDependenceModule::init(uint32_t loop_id, uint32_t pid) {
  // FIXME: implement init with number of loops and instructions
  uint32_t num_instrs = 4000;
  uint32_t num_loops = 275;

  loop_hierarchy = new Loops();
  memoryProfiler = new MemoryProfilerType(num_instrs);

  LoopInfoType &loopInfo = loop_hierarchy->getCurrentLoop();
  DependenceSets dependenceSets(MemoryProfilerType::MAX_TRACKED_DISTANCE);

  loop_hierarchy->loopIteration(0, iterationcount);

  loopInfo.setItem(dependenceSets);

  time_stamp = 1;

  giNumLoops = num_loops + 1;
  iterationcount = (uint64_t *)calloc(giNumLoops, sizeof(uint64_t));

  smmap->init_stack(SIZE_8M, pid);
}

// static uint64_t log_time = 0;
void WholeProgramDependenceModule::fini(const char *filename) {
  // FIXME: implement fini

  auto lamp_out = new ofstream("result.lamp.profile");

  // Print out Loop Iteration Counts
  for (unsigned i = 0; i < giNumLoops; i++) {
    *lamp_out << i << " " << iterationcount[i] << "\n";
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
}

static void log(const timestamp_t ts, const uint32_t dst_inst,
                const uint32_t context) {
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
                                        const uint32_t bare_instr) {
  local_write(addr, [&]() {
    TS *s = (TS *)GET_SHADOW(addr, DM_TIMESTAMP_SIZE_IN_BYTES_LOG2);

    timestamp_ts_u ts;
    ts.ts = s[0];

    // FIXME: fix the type of the timestamp here
    if (ts.ts != 0) {
      log(ts.timestamp, instr, context);
    }
  });
}

void WholeProgramDependenceModule::store(uint32_t instr, uint32_t bare_instr,
                                         const uint64_t addr) {
  local_write(addr, [&]() {
    // store_count++;
    TS *shadow_addr = (TS *)GET_SHADOW(addr, DM_TIMESTAMP_SIZE_IN_BYTES_LOG2);

    if (context != 0) {
      instr = context;
    }

    timestamp_ts_u ts;
    ts.timestamp.instr = instr;
    ts.timestamp.timestamp = time_stamp;

    // FIXME: Replace it with actual timestamp
    shadow_addr[0] = ts.ts;
  });
}

void WholeProgramDependenceModule::loop_invoc(uint32_t loop_id) {
  // FIXME: takes the ID
  loop_hierarchy->enterLoop(loop_id, time_stamp);
  initializeSets();

}

void WholeProgramDependenceModule::loop_iter() {
  time_stamp++;
  loop_hierarchy->loopIteration(time_stamp, iterationcount);
  initializeSets();
}

void WholeProgramDependenceModule::loop_exit(uint32_t loop_id) {
  // FIXME: takes the ID
  loop_hierarchy->exitLoop(loop_id);
}

void WholeProgramDependenceModule::merge_dep(
    WholeProgramDependenceModule &other) {
  // FIXME: implement merge
}
