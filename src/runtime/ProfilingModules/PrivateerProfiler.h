#include <cstdint>
#include <unordered_set>
#include <vector>

#include "ContextManager.h"
#include "HTContainer.h"
#include "LocalWriteModule.h"

class PrivateerProfiler : public LocalWriteModule {
private:
  uint32_t target_loop_id = 0;

public:
  PrivateerProfiler(uint32_t mask, uint32_t pattern)
      : LocalWriteModule(mask, pattern) {
    // smmap = new slamp::MemoryMap<MASK2_PT>(mask, pattern,
    // TIMESTAMP_SIZE_IN_BYTES);
  }

  ~PrivateerProfiler() override {
    // delete smmap;
  }

  void init(uint32_t loop_id, uint32_t pid);
  void fini(const char *filename);

  void load(uint32_t instr, uint64_t value);

  void allocate(void *addr, uint32_t instr, uint64_t size);
  void realloc(void *old_addr, void *new_addr, uint32_t instr, uint64_t size);
  void free(void *addr);
  void stack_alloc(void *addr, uint32_t instr, uint64_t size);
  void stack_free(void *addr);

  void func_entry(uint32_t fcnId);
  void func_exit(uint32_t fcnId);
  void loop_entry(uint32_t loopId);
  void loop_exit(uint32_t loopId);
  void loop_iter(uint32_t loopId);

  void predict_int(uint32_t instId, uint64_t value);
  void predict_ptr(uint32_t instId, void *ptr);

  void points_to_inst(uint32_t instId, void *ptr);
  void points_to_arg(uint32_t fcnId, uint32_t argId, void *ptr);
  //   void merge(PointsToModule &other);
  //   void decode_all();
};
