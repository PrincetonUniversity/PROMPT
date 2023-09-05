#include "gtest/gtest.h"

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/LLVMContext.h"

#include "CastUtil.h"

TEST(CastUtilTest, CastToInt64Ty) {
  llvm::LLVMContext context;
  llvm::IRBuilder<> builder(context);

  // Create a 32-bit integer value
  llvm::Value *constInt = llvm::ConstantInt::get(context, llvm::APInt(32, 42));
  llvm::Value *inst = builder.CreateAdd(constInt, constInt);

  // Create an instruction insertion point
  liberty::InstInsertPt out =
      liberty::InstInsertPt::Before(llvm::dyn_cast<llvm::Instruction>(inst));

  // Cast the value to a 64-bit integer
  llvm::Value *result = liberty::castToInt64Ty(constInt, out, nullptr);

  // Check that the result is a 64-bit integer
  ASSERT_TRUE(result->getType()->isIntegerTy(64));
}