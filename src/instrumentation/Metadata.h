#ifndef LLVM_LIBERTY_METADATA_MANAGER
#define LLVM_LIBERTY_METADATA_MANAGER

#include "llvm/IR/PassManager.h"

namespace liberty {
using namespace llvm;

class Namer : public PassInfoMixin<Namer> {
private:
  Module *pM;
  int funcId;
  int blkId;
  int instrId;

public:
  Namer();
  ~Namer();

  void reset();
  llvm::PreservedAnalyses run(llvm::Module &M,
                              llvm::ModuleAnalysisManager &MAM);

  bool runOnFunction(Function &F);
  static Value *getFuncIdValue(Instruction *I);
  static Value *getBlkIdValue(Instruction *I);
  static Value *getInstrIdValue(Instruction *I);
  static Value *getInstrIdValue(const Instruction *I);
  static int getFuncId(Function *F);
  static int getFuncId(Instruction *I);
  static int getBlkId(BasicBlock *I);
  static int getBlkId(Instruction *I);
  static int getInstrId(Instruction *I);
  static int getInstrId(const Instruction *I);
};
} // namespace liberty
#endif
