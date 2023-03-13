#pragma once
#include <cstdint>

class ProfilingModule {
  public:
    virtual ~ProfilingModule() = default;

    virtual void init(uint32_t loop_id, uint32_t pid) = 0;
    virtual void fini(const char *filename) = 0;
    virtual void load(uint32_t instr, const uint64_t addr,
                      const uint32_t bare_instr) = 0;
    virtual void store(uint32_t instr, const uint64_t addr) = 0;
    virtual void allocate(void *addr, uint32_t instr, uint64_t size) = 0;
    virtual void free(void *addr, uint32_t instr = 0) = 0;
    virtual void loop_invoc() = 0;
    virtual void loop_iter() = 0;
    virtual void loop_exit() = 0;
    virtual void func_entry(uint32_t context) = 0;
    virtual void func_exit(uint32_t context) = 0;
    virtual void loop_entry(uint32_t loop_id) = 0;
    virtual void loop_exit(uint32_t loop_id) = 0;
    virtual void loop_iter_ctx(uint32_t loop_id) = 0;
    virtual void points_to_arg(uint32_t arg_id, uint64_t addr) = 0;
    virtual void points_to_inst(uint32_t instr, uint64_t addr) = 0;

    virtual void merge_dep(ProfilingModule &other) = 0;
};
