#include "ProfilingModule.h"
#include "slamp_consume.h"
#include <cstdint>
#include <cstdio>
#include <iostream>

#define ATTRIBUTE(x) __attribute__((x))
#define DEBUG 0
#define ACTION 1
#define MEASURE_TIME 0

static uint64_t load_time(0);
static uint64_t store_time(0);
static uint64_t alloc_time(0);

static inline uint64_t rdtsc() {
  uint64_t a, d;
  __asm__ volatile("rdtsc" : "=a"(a), "=d"(d));
  return (d << 32) | a;
}

// TODO: make it templated, takes in the declaration file and template it out
// either through a python script or a small separate C++ program per module
void consume_loop(ProfilingModule &mod) ATTRIBUTE(noinline) {
  uint64_t rdtsc_start = 0;
  uint64_t counter = 0;
  uint32_t loop_id;

  // measure time with lambda action
  auto measure_time = [](uint64_t &time, auto action) {
    // measure time with rdtsc
    if (MEASURE_TIME) {
      uint64_t start = rdtsc();
      action();
      uint64_t end = rdtsc();
      time += end - start;
    } else {
      action();
    }
  };

  while (true) {
    uint32_t v;
    CONSUME_PACKET(v);

    counter++;

    // convert v to action
    auto action = static_cast<Action>(v);

    switch (action) {
    // init(loop_id, pid)
    case Action::INIT: {
      uint32_t pid;
      rdtsc_start = rdtsc();

      if (DEBUG) {
        std::cout << "INIT: " << loop_id << " " << pid << std::endl;
      }
      if (ACTION) {
        mod.init(loop_id, pid);
      }
      break;
    };

    // alloc(instr, addr, size)
    case Action::ALLOC: {
      uint32_t instr;
      uint64_t addr;
      uint32_t size;
      CONSUME_ALLOC(instr, addr, size);

      if (DEBUG) {
        std::cout << "ALLOC: " << instr << " " << addr << " " << size
                  << std::endl;
      }

      if (ACTION) {
        measure_time(alloc_time, [&]() {
          mod.allocate(reinterpret_cast<void *>(addr), instr, size);
        });
      }
      break;
    };

    // free(addr)
    case Action::FREE: {
      uint64_t addr;
      CONSUME_FREE(addr);

      if (DEBUG) {
        std::cout << "FREE: " << addr << std::endl;
      }
      if (ACTION) {
        measure_time(alloc_time,
                     [&]() { mod.free(reinterpret_cast<void *>(addr)); });
      }
      break;
    };

    // loop_invoc()
    case Action::TARGET_LOOP_INVOC: {
      CONSUME_TARGET_LOOP_INVOC();
      if (DEBUG) {
        std::cout << "TARGET_LOOP_INVOC" << std::endl;
      }

      if (ACTION) {
        mod.loop_invoc();
      }
      break;
    };

    // loop_iter()
    case Action::TARGET_LOOP_ITER: {
      CONSUME_TARGET_LOOP_ITER();
      if (DEBUG) {
        std::cout << "TARGET_LOOP_ITER" << std::endl;
      }
      if (ACTION) {
        mod.loop_iter();
      }
      break;
    };

    // target_loop_exit()
    case Action::TARGET_LOOP_EXIT: {
      CONSUME_TARGET_LOOP_EXIT();
      if (DEBUG) {
        std::cout << "TARGET_LOOP_EXIT " << std::endl;
      }
      if (ACTION) {
        mod.loop_exit();
      }
      break;
    };

    // loop_exit(loop_id)
    case Action::LOOP_EXIT: {
      CONSUME_LOOP_EXIT(loop_id);
      if (DEBUG) {
        std::cout << "LOOP_EXIT " << std::endl;
      }
      if (ACTION) {
        mod.loop_exit();
      }
      break;
    };

    // func_entry(func_id)
    case Action::FUNC_ENTRY: {
      uint32_t func_id;
      CONSUME_FUNC_ENTRY(func_id);
      if (DEBUG) {
        std::cout << "FUNC_ENTRY: " << func_id << std::endl;
      }
      if (ACTION) {
        mod.func_entry(func_id);
      }
      break;
    };

    // func_exit(func_id)
    case Action::FUNC_EXIT: {
      uint32_t func_id;
      CONSUME_FUNC_EXIT(func_id);
      if (DEBUG) {
        std::cout << "FUNC_EXIT: " << func_id << std::endl;
      }
      if (ACTION) {
        mod.func_exit(func_id);
      }
      break;
    };

    // load(instr, addr, value)
    case Action::LOAD: {
      uint32_t instr;
      uint64_t addr;
      uint64_t value;
      CONSUME_LOAD(instr, addr, value);

      if (DEBUG) {
        std::cout << "LOAD: " << instr << " " << addr << " " << value
                  << std::endl;
      }
      if (ACTION) {
        measure_time(load_time, [&]() { mod.load(instr, addr, value); });
      }

      break;
    };

    // store(instr, addr)
    case Action::STORE: {
      uint32_t instr;
      uint64_t addr;
      CONSUME_STORE(instr, addr);

      if (DEBUG) {
        std::cout << "STORE: " << addr << std::endl;
      }

      if (ACTION) {
        measure_time(store_time, [&]() { mod.store(instr, addr); });
      }

      break;
    };

    // loop_entry(loop_id)
    case Action::LOOP_ENTRY: {
      CONSUME_LOOP_ENTRY(loop_id);

      if (DEBUG) {
        std::cout << "LOOP_ENTRY: " << loop_id << std::endl;
      }

      if (ACTION) {
        mod.loop_entry(loop_id);
      }
      break;
    };

    // loop_entry(loop_id)
    case Action::LOOP_ITER_CTX: {
      CONSUME_LOOP_ITER_CTX(loop_id);
      if (DEBUG) {
        std::cout << "LOOP_ITER_CTX: " << loop_id << std::endl;
      }
      if (ACTION) {
        mod.loop_iter_ctx(loop_id);
      }
      break;
    };

    // points_to_inst(inst_id, addr)
    case Action::POINTS_TO_INST: {
      uint32_t inst_id;
      uint64_t addr;
      CONSUME_POINTS_TO_INST(inst_id, addr);

      if (DEBUG) {
        std::cout << "POINTS_TO_INST: " << inst_id << " " << addr << std::endl;
      }

      if (ACTION) {
        mod.points_to_inst(inst_id, addr);
      }
      break;
    };

    // points_to_arg(arg_id, addr)
    case Action::POINTS_TO_ARG: {
      uint32_t arg_id;
      uint64_t addr;
      CONSUME_POINTS_TO_ARG(arg_id, addr);
      if (DEBUG) {
        std::cout << "POINTS_TO_ARG: " << arg_id << " " << addr << std::endl;
      }
      if (ACTION) {
        mod.points_to_arg(arg_id, addr);
      }
      break;
    };

    // finished()
    case Action::FINISHED: {
      CONSUME_FINISHED();
      uint64_t rdtsc_end = rdtsc();
      // total cycles
      uint64_t total_cycles = rdtsc_end - rdtsc_start;
      std::cout << "Finished loop: " << loop_id << " after " << counter
                << " events" << std::endl;
      // print time in seconds
      std::cout << "Total time: " << total_cycles / 2.6e9 << " s" << std::endl;
      if (MEASURE_TIME) {
        std::cout << "Load time: " << load_time / 2.6e9 << " s" << std::endl;
        std::cout << "Store time: " << store_time / 2.6e9 << " s" << std::endl;
        std::cout << "Alloc time: " << alloc_time / 2.6e9 << " s" << std::endl;
      }

      if (ACTION) {
        mod.fini("ollog.txt");
      }

      return;
    };
    default: {
      CONSUME_QUEUE_DEBUG();
      // std::cout << "Unknown action: " << (uint64_t)v << std::endl;
      // std::cout << "Is ready to read?:" << dq.qNow->ready_to_read << " "
      //           << "Is ready to write?:" << dq.qNow->ready_to_write <<
      //           std::endl;
      // std::cout << "Index: " << dq.index << " Size:" << dq.qNow->size <<
      // std::endl;

      // for (int i = 0; i < 101; i++) {
      //   std::cout << dq.qNow->data[dq.index - 100 + i] << " ";
      // }
      exit(-1);
    }
    }
  }
}
