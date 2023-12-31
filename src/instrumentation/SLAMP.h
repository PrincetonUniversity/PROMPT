#ifndef LLVM_LIBERTY_SLAMP_SLAMP_H
#define LLVM_LIBERTY_SLAMP_SLAMP_H

#include "llvm/IR/Constants.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"

#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/DataLayout.h"

#include <set>
#include <unordered_set>

namespace liberty::slamp {

using namespace std;
using namespace llvm;

class SLAMP : public ModulePass {
  friend class MemoryMeasure;

public:
  static char ID;
  SLAMP();
  ~SLAMP();

  void getAnalysisUsage(AnalysisUsage &au) const;

  bool runOnModule(Module &m);

private:
  bool findTarget(Module &m);

  bool mayCallSetjmpLongjmp(Loop *loop);
  void getCallableFunctions(Loop *loop, set<Function *> &callables);
  void getCallableFunctions(Function *f, set<Function *> &callables);
  void getCallableFunctions(CallInst *ci, set<Function *> &callables);
  void getFunctionsWithSign(CallInst *ci, set<Function *> matched);

  void replaceExternalFunctionCalls(Module &m);

  Function *instrumentConstructor(Module &m);
  void instrumentDestructor(Module &m);
  void instrumentGlobalVars(Module &m, Function *ctor);
  void instrumentAllocas(Module &m);
  void instrumentBasePointer(Module &m);

  // functions used in instrumentAllocas
  void findLifetimeMarkers(Value *i, set<const Value *> &already,
                           std::vector<Instruction *> &starts,
                           std::vector<Instruction *> &ends);
  void reportStartOfAllocaLifetime(AllocaInst *inst, Instruction *start,
                                   Function *fcn, const DataLayout &dl,
                                   Module &m);
  void reportEndOfAllocaLifetime(AllocaInst *inst, Instruction *end, bool empty,
                                 Function *fcn, Module &m);

  void instrumentNonStandards(Module &m, Function *ctor);
  void allocErrnoLocation(Module &m, Function *ctor);
  void instrumentLoopStartStopForAll(Module &m);
  void instrumentFunctionStartStop(Module &m);
  void instrumentLoopStartStop(Module &m, Loop *l);
  void instrumentInstructions(Module &m, Loop *l = nullptr);

  void instrumentMainFunction(Module &m);

  static int getIndex(PointerType *ty, size_t &size, const DataLayout &DL);
  void instrumentMemIntrinsics(Module &m, MemIntrinsic *mi);
  void instrumentLifetimeIntrinsics(Module &m, Instruction *inst);
  void instrumentLoopInst(Module &m, Instruction *inst, uint32_t id);
  void instrumentExtInst(Module &m, Instruction *inst, uint32_t id);

  void addWrapperImplementations(Module &m);

  // frequently used types

  Type *Void, *I32, *I64, *I8Ptr;

  Function *target_fn;
  Loop *target_loop;
  unordered_set<Instruction *> elidedLoopInsts;
};

} // namespace liberty::slamp

#endif
