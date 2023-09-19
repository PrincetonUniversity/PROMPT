#ifndef LOOP_HIERARCHY_H
#define LOOP_HIERARCHY_H

#include <cassert>
#include <cinttypes>
#include <cstdlib>
#include <iostream>
#include <iterator>
#include <vector>

// FIXME: use more modern circular buffer implementation (maybe Boost)
#include "CircularQueue.h"

#define DEBUG 0

using namespace std;
using namespace Collections;

namespace Loop {

static const uint64_t DEFAULT_DEPENDENCE_DISTANCE = 5;

template <class T, int maxDepDist = DEFAULT_DEPENDENCE_DISTANCE>
class LoopInfo {
public:
  circular_buffer<uint64_t> iteration_time_stamps;
  uint64_t invocation_time_stamp{};
  uint32_t loop_id{};
  uint64_t iters{}; // TRM
  T item;

  LoopInfo() : iteration_time_stamps(maxDepDist), item() {}

  void reset(uint64_t loop, uint64_t time_stamp) {
    this->loop_id = loop;
    this->invocation_time_stamp = time_stamp;
    this->iteration_time_stamps.clear();
  }

  T &getItem() { return item; }

  void setItem(const T &item) { this->item = item; }

  void iteration(uint64_t time_stamp) {
    this->iteration_time_stamps.push_back(time_stamp);
  }
};

static const uint64_t DEFAULT_LOOP_DEPTH = 1048576;

template <class T, int maxLoopDepth = DEFAULT_LOOP_DEPTH,
          int maxDepDistance = DEFAULT_DEPENDENCE_DISTANCE>
class LoopHierarchy {
public:
  using LoopInfoType = LoopInfo<T, maxDepDistance>;

  using iterator = typename vector<LoopInfoType>::iterator;

  using reverse_iterator = typename vector<LoopInfoType>::reverse_iterator;

  uint32_t max_depth{};

  uint32_t current_depth{static_cast<uint32_t>(-1)};

  vector<LoopInfoType> loop_info;

  LoopHierarchy() : loop_info(maxLoopDepth) { enterLoop(0, 0); }

  void enterLoop(uint64_t loop_id, uint64_t timestamp) {
    this->current_depth++;
    this->loop_info.at(this->current_depth).reset(loop_id, timestamp);

#if DEBUG
    cerr << "Entering loop " << loop_id << " now at depth "
         << this->current_depth << "\n";
#endif
    if (this->current_depth > this->max_depth) {
      this->max_depth = this->current_depth;
    }
  }

  void exitLoop(const uint16_t loop_id) {
#if DEBUG
    if (this->loop_info.at(this->current_depth).loop_id != loop_id) {

      cerr << "ERROR: Exiting from loop " << loop_id << " but expected loop "
           << this->loop_info.at(this->current_depth).loop_id << endl;
      exit(-1);
    } else {
      cerr << "Exiting from loop " << loop_id << endl;
    }
#endif

    // TODO: check the loop_id
    this->current_depth--;
#if DEBUG
    cerr << "Leaving a loop, now at depth " << this->current_depth << "\n";
    assert(this->current_depth <= 1048 && "We left a loop we never entered");
#endif
  }

  void loopIteration(uint64_t time_stamp) {
    this->getCurrentLoop().iteration(time_stamp);
  }

  LoopInfoType &getCurrentLoop() {
    return this->loop_info.at(this->current_depth);
  }

  LoopInfoType &findLoop(uint64_t store_time_stamp) {
    if (store_time_stamp == 0) {
      return loop_info[0];
    }

    for (uint32_t iter = this->current_depth; iter > 0; iter--) {
      if (loop_info[iter].invocation_time_stamp < store_time_stamp)
        return loop_info[iter];
    }

    if (this->loop_info[0].invocation_time_stamp > store_time_stamp) {
      cerr << "Unexpected time stamp: "
           << this->loop_info[0].invocation_time_stamp << " > "
           << store_time_stamp << endl;
      abort();
    }

    return this->loop_info[0];
  }

  uint32_t calculateDistance(LoopInfoType &store_loop,
                             uint64_t store_time_stamp) {
    circular_buffer<uint64_t>::reverse_iterator iter =
        store_loop.iteration_time_stamps.rbegin();

    uint32_t distance = 0;
    for (; iter != store_loop.iteration_time_stamps.rend(); iter++) {
      if (*iter <= store_time_stamp)
        break;
      distance++;
    }

    return distance;
  }
};
} // namespace Loop

#endif
