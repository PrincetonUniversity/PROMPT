#ifndef CAST_UTIL_H
#define CAST_UTIL_H

#include "llvm/IR/Instruction.h"
#include "utils/InstInsertPt.h"
#include <vector>

namespace liberty {

typedef std::vector<llvm::Instruction *> NewInstructions;

llvm::Value *castToInt64Ty(llvm::Value *value, liberty::InstInsertPt &out,
                           NewInstructions *newInstructions = 0);

llvm::Value *castIntToInt32Ty(llvm::Value *value, liberty::InstInsertPt &out,
                              NewInstructions *newInstructions = 0);

llvm::Value *castFromInt64Ty(llvm::Type *ty, llvm::Value *value,
                             liberty::InstInsertPt &out,
                             NewInstructions *newInstructions = 0);

llvm::Value *castPtrToVoidPtr(llvm::Value *value, liberty::InstInsertPt &out,
                              NewInstructions *newInstructions = 0);
} // namespace liberty

#endif /* CAST_UTIL_H */
