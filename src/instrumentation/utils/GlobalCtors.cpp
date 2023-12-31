#include <vector>

#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"

#include "utils/GlobalCtors.h"

namespace liberty {
using namespace llvm;

// Wrap a function pointer in an array of structures so that it
// is the type required for a global ctor list or global dtor list
// then append it to those lists
static void appendToConstructorArray(Function *f, const std::string &name,
                                     const unsigned int priority = 65535,
                                     const bool ascending = true) {

  LLVMContext &Context = f->getParent()->getContext();

  assert(f->arg_size() == 0 &&
         "Cannot pass arguments to a function ``before main''");

  Module *module = f->getParent();
  assert(module && "Function has not yet been added to a module");

  // Several types we will use often.
  // We give them friendly names here.
  // int
  Type *intType = Type::getInt32Ty(Context);

  Type *int8Type = Type::getInt8Ty(Context);
  PointerType *int8PtrType = Type::getInt8PtrTy(Context);

  // void fcn(void)
  std::vector<Type *> formals(0);
  FunctionType *voidFcnVoidType =
      FunctionType::get(Type::getVoidTy(Context), formals, false);

  // The type of a global constructor
  // record, llvm calls this "{ i32, void () *, i8* }"
  std::vector<Type *> fieldsty(3);
  fieldsty[0] = intType;
  fieldsty[1] = PointerType::getUnqual(voidFcnVoidType);
  fieldsty[2] = PointerType::getUnqual(int8Type);
  StructType *ctorRecordType = StructType::get(f->getContext(), fieldsty);

  // Build a new constructor list
  std::vector<Constant *> elts;

  // keep old list if any, append it at the end of our new constructor list
  // (after our startup function)
  std::vector<Constant *> old_elts;

  // If there was already a constructor list...
  GlobalVariable *previous = module->getGlobalVariable(name);
  if (previous) {

    // and it has an initializer
    if (previous->hasInitializer()) {

      // and if that initializer was an array
      if (ConstantArray *ca =
              dyn_cast<ConstantArray>(previous->getInitializer())) {

        // We might need to create a driver function to assert priorities.
        Function *driver = 0;
        BasicBlock *entry = 0;

        // Copy over it's elements
        for (User::op_iterator i = ca->op_begin(), e = ca->op_end(); i != e;
             ++i) {

          ConstantStruct *oldRec = cast<ConstantStruct>(i->get());

          ConstantInt *prio =
              cast<ConstantInt>(oldRec->getAggregateElement(0u));

          if (((prio->getZExtValue() > priority) && ascending) ||
              ((prio->getZExtValue() < priority) && !ascending)) {
            if (!driver) {
              // Create a new driver function that will call US before them.
              driver = Function::Create(voidFcnVoidType,
                                        GlobalValue::InternalLinkage,
                                        "callBeforeMain_driver.", module);
              entry = BasicBlock::Create(Context, "entry", driver);
              CallInst::Create(f, "", entry);
              f = driver;
            }

            Function *initor = cast<Function>(oldRec->getAggregateElement(1));
            CallInst::Create(initor, "", entry);
          } else {
            old_elts.push_back(oldRec);
          }
        }

        if (driver)
          ReturnInst::Create(Context, entry);
      }
    }

    // and delete the old one
    previous->eraseFromParent();
  }

  // (global constructor record value)
  std::vector<Constant *> fields(3);
  fields[0] = ConstantInt::get(intType, priority);
  fields[1] = f;
  fields[2] = ConstantPointerNull::get(int8PtrType);
  ;
  Constant *record = ConstantStruct::get(ctorRecordType, fields);

  // Add our new elements
  elts.push_back(record);

  // Append elements of the old contructor list at the end of the new list
  // Appending in the beginning leads to early abort
  for (auto el : old_elts)
    elts.push_back(el);

  // Add it to the module
  Constant *array =
      ConstantArray::get(ArrayType::get(ctorRecordType, elts.size()), elts);

  new GlobalVariable(*module, array->getType(), false,
                     GlobalValue::AppendingLinkage, array, name);
}

// void callBeforeMain( Function *f, const unsigned int priority = 65535 ) {
void callBeforeMain(Function *f, const unsigned int priority) {
  appendToConstructorArray(f, "llvm.global_ctors", priority, true);
}

// void callAfterMain( Function *f, const unsigned int priority = 65535 ) {
void callAfterMain(Function *f, const unsigned int priority) {
  appendToConstructorArray(f, "llvm.global_dtors", priority, false);
}

} // namespace liberty
