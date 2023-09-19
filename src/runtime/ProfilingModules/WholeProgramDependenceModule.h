#include <cstdint>
#include <mutex>
#include <unordered_set>
#include <vector>

#include "parallel_hashmap/phmap.h"
#include "slamp_shadow_mem.h"
#include "slamp_timestamp.h"

#include "HTContainer.h"
#include "LocalWriteModule.h"
#include "LoopHierarchy.h"
#include "MemoryProfile.h"
#include "Profile.h"

#define DM_TIMESTAMP_SIZE_IN_BYTES 8
#define DM_TIMESTAMP_SIZE_IN_BYTES_LOG2 3

using namespace Profiling;
using namespace Loop;

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

constexpr uint64_t MAX_DEP_DIST = 2;

using DependenceSets = vector<DependenceSet>;
using Loops =
    LoopHierarchy<DependenceSets, Loop::DEFAULT_LOOP_DEPTH, MAX_DEP_DIST>;
using LoopInfoType = Loops::LoopInfoType;
using MemoryProfilerType = MemoryProfiler<MAX_DEP_DIST>;

class WholeProgramDependenceModule : public LocalWriteModule {
private:
  uint64_t slamp_iteration = 0;
  uint64_t slamp_invocation = 0;

  // debugging stats
  uint64_t load_count = 0;
  uint64_t store_count = 0;

  int nested_level = 0;

  slamp::MemoryMap<MASK2> *smmap = nullptr;

  uint32_t giNumLoops;

  lamp_stats_t lamp_stats;

  MemoryProfilerType *memoryProfiler;
  Loops *loop_hierarchy;

  phmap::flat_hash_map<uint32_t, uint32_t> loop_iteration_count;

  uint64_t time_stamp;
  void initializeSets();
  LoopInfoType &fillInDependence(const timestamp_t value, Dependence &dep);
  void log(const timestamp_t ts, const uint32_t dst_inst);
  // void log(TS ts, const uint32_t dst_inst, const uint32_t bare_inst);

public:
  WholeProgramDependenceModule(uint32_t mask, uint32_t pattern)
      : LocalWriteModule(mask, pattern) {
    smmap =
        new slamp::MemoryMap<MASK2>(mask, pattern, DM_TIMESTAMP_SIZE_IN_BYTES);
  }

  ~WholeProgramDependenceModule() override { delete smmap; }

  void init(uint32_t loop_id, uint32_t pid);
  void fini(const char *filename);
  // always_inline attribute
  void load(uint32_t instr, const uint64_t addr, const uint32_t bare_instr, const uint32_t size);
  void store(uint32_t instr, uint32_t bare_instr, const uint64_t addr, const uint32_t size);
  void allocate(void *addr, uint64_t size);
  void loop_entry(uint32_t loop_id);
  void loop_iter();
  void loop_exit(uint32_t loop_id);

  void merge_dep(WholeProgramDependenceModule &other);
};
