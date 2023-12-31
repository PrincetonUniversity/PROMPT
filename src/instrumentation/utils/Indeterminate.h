// This class is responsible for determining
// which objects cannot be determined at compile
// time.  It is used to decide which to instrument,
// and to determine (when reading profile info)
// when we expect a value to be instrumented
#ifndef LLVM_LIBERTY_SPEC_PRIV_INDETERMINATE_H
#define LLVM_LIBERTY_SPEC_PRIV_INDETERMINATE_H

#include "llvm/IR/Instruction.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/CommandLine.h"

#include "utils/FindUnderlyingObjects.h"

namespace liberty {
namespace SpecPriv {
using namespace llvm;

extern cl::opt<bool> SanityCheckMode;

struct Indeterminate {
  static void findIndeterminateObjects(BasicBlock &bb, UO &objects);
  static void findIndeterminateObjects(BasicBlock &bb, UO &pointers,
                                       UO &objects);

  static void findIndeterminateObjects(Value *ptr, UO &pointers, UO &objects,
                                       const DataLayout &DL);
  static void findIndeterminateObject(const Value *ptr, UO &objects);

  // Functions which allocate memory
  static bool isMallocOrCalloc(const Value *inst);
  static bool isMalloc(const Value *inst);
  static bool isCalloc(const Value *inst);
  static bool isRealloc(const Value *inst);

  static bool isMallocOrCalloc(const CallBase &cs);
  static bool isMalloc(const CallBase &cs);
  static bool isNewNoThrow(const CallBase &cs);
  static bool isCalloc(const CallBase &cs);
  static bool isRealloc(const CallBase &cs);

  // Functions which free memory
  static bool isFree(const Value *inst);
  static bool isFree(const CallBase &cs);

  // Functions which allocate a FILE*
  static bool isFopen(const Value *inst);
  static bool isFopen(const CallBase &cs);
  static bool isFdopen(const CallBase &cs);
  static bool isFreopen(const CallBase &cs);
  static bool isPopen(const CallBase &cs);
  static bool isTmpfile(const CallBase &cs);
  static bool isOpendir(const CallBase &cs);
  static bool returnsNewFilePointer(const CallBase &cs);

  // Functions which free a FILE*
  static bool isFclose(const Value *inst);
  static bool isFclose(const CallBase &cs);
  static bool isClosedir(const CallBase &cs);
  static bool closesFilePointer(const CallBase &cs);

  // Functions which return a pointer to a constant, null-terminated
  // buffer located inside of a library.
  static bool returnsLibraryConstantString(const CallBase &cs);
};

} // namespace SpecPriv
} // namespace liberty

#endif
