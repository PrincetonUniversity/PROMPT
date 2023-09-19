#ifndef MEMORY_PROFILING_H
#define MEMORY_PROFILING_H

#include <inttypes.h>
#include <iostream>
#include <ostream>

#include <iterator>
#include <map>
#include <vector>

#include "Profile.h"

using namespace std;

namespace Profiling {
class MemoryProfile {
private:
  uint64_t total_count;
  uint64_t loop_count;

public:
  MemoryProfile() : total_count(0), loop_count(0) {}

  void increment() { total_count++; }

  void incrementLoop() { loop_count++; }

  uint64_t getCount() { return total_count; }
  uint64_t getLoopCount() { return loop_count; }

  friend std::ostream &operator<<(std::ostream &stream,
                                  const MemoryProfile &vp);
};

template <int maxTrackedDistance = DEFAULT_TRACKED_DISTANCE>
class MemoryProfiler
    : public KeyDistanceProfiler<MemoryProfile, maxTrackedDistance> {
public:
  MemoryProfiler(const uint32_t num_instrs)
      : KeyDistanceProfiler<MemoryProfile, maxTrackedDistance>(num_instrs) {}

  MemoryProfile &increment(const Dependence &dep) {
    MemoryProfile &profile = this->getProfile(dep);
    profile.increment();
    return profile;
  }

  template <int S>
  friend ostream &operator<<(ostream &stream, const MemoryProfiler<S> &vp);
};

template <int S>
ostream &operator<<(ostream &stream, const MemoryProfiler<S> &vp) {
  stream << "BEGIN Memory Profile" << endl;
  stream << ((KeyDistanceProfiler<MemoryProfile, S> &)vp);
  stream << "END Memory Profile" << endl;
  return stream;
}
} // namespace Profiling

#endif
