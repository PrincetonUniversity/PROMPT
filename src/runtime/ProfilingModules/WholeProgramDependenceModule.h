#include <cstdint>
#include <mutex>
#include <unordered_set>
#include <vector>

#include "slamp_logger.h"
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

static const uint64_t MAX_DEP_DIST = 2;

using DependenceSets = vector<DependenceSet>;
using Loops =
    LoopHierarchy<DependenceSets, Loop::DEFAULT_LOOP_DEPTH, MAX_DEP_DIST>;
using LoopInfoType = Loops::LoopInfoType;

class WholeProgramDependenceModule : public LocalWriteModule {
private:
  uint64_t slamp_iteration = 0;
  uint64_t slamp_invocation = 0;

  // debugging stats
  uint64_t load_count = 0;
  uint64_t store_count = 0;

  unsigned int context = 0;
  int nested_level = 0;

  slamp::MemoryMap<MASK2> *smmap = nullptr;

  phmap::flat_hash_set<slamp::KEY, slamp::KEYHash, slamp::KEYEqual> deps;

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
  void load(uint32_t instr, const uint64_t addr, const uint32_t bare_instr);
  void store(uint32_t instr, uint32_t bare_instr, const uint64_t addr);
  void allocate(void *addr, uint64_t size);
  void loop_invoc(uint32_t loop_id);
  void loop_iter();
  void loop_exit(uint32_t loop_id);

  void merge_dep(WholeProgramDependenceModule &other);
};
