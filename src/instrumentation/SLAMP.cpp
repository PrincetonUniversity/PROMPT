//===- SLAMP.cpp - Insert SLAMP instrumentation -----------===//
//
// Single Loop Aware Memory Profiler.
//

#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Instructions.h"
#include <cstdint>
#define DEBUG_TYPE "SLAMP"

#include "llvm/IR/LLVMContext.h"
#include "llvm/Support/raw_ostream.h"

// #define USE_PDG

#ifdef USE_PDG
#include "scaf/SpeculationModules/PDGBuilder.hpp"
#include "scaf/Utilities/PDGQueries.h"
#endif

#include "llvm/ADT/Statistic.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"

#include "utils/CastUtil.h"
#include "utils/GlobalCtors.h"
#include "utils/Indeterminate.h"
#include "utils/InstInsertPt.h"

#include "Metadata.h"
#include "SLAMP.h"

#include "externs.h"

#include <map>
#include <sstream>
#include <vector>

#define INST_ID_BOUND (((uint32_t)1 << 20) - 1)

using namespace std;
using namespace llvm;

static std::map<std::string, Constant *> slemap;

Constant *getStringLiteralExpression(Module &m, const std::string &str) {
  if (slemap.count(str))
    return slemap[str];

  LLVMContext &Context = m.getContext();

  Constant *array = ConstantDataArray::getString(Context, str);

  GlobalVariable *strConstant =
      new GlobalVariable(m, array->getType(), true, GlobalValue::PrivateLinkage,
                         array, "__" + str);

  Constant *zero = ConstantInt::get(Type::getInt64Ty(Context), 0);
  Value *zeros[] = {zero, zero};
  ArrayRef<Value *> zerosRef(zeros, zeros + 2);

  // TODO: double-check passed type
  slemap[str] = ConstantExpr::getInBoundsGetElementPtr(
      strConstant->getType()->getPointerElementType(), strConstant, zerosRef);
  return slemap[str];
}

namespace liberty::slamp {

char SLAMP::ID = 0;
static uint64_t numElidedNode = 0;
static uint64_t numInstrumentedNode = 0;

// FIXME: hack for supporting LAMP
STATISTIC(numElidedNodeStats, "Number of instructions in the loop that are "
                              "ignored for SLAMP due to pruning");
STATISTIC(numInstrumentedNodeStats,
          "Number of instructions in the loop that are instrumented ");
static RegisterPass<SLAMP> RP("slamp-insts",
                              "Insert instrumentation for SLAMP profiling",
                              false, false);

// whether to enable targeting a specific loop
static cl::opt<bool> TargetLoopEnabled("slamp-target-loop-enabled",
                                       cl::init(true), cl::NotHidden,
                                       cl::desc("Target Loop Enabled"));

// passed in based on the target loop info
static cl::opt<std::string> TargetFcn("slamp-target-fn", cl::init(""),
                                      cl::NotHidden,
                                      cl::desc("Target Function"));

// passed in based on the target loop info
static cl::opt<std::string> TargetLoop("slamp-target-loop", cl::init(""),
                                       cl::NotHidden, cl::desc("Target Loop"));

// top priority; not compatible with the rest
static cl::list<uint32_t>
    ExplicitInsts("slamp-explicit-insts", cl::NotHidden, cl::CommaSeparated,
                  cl::desc("Explicitly instrumented instructions"),
                  cl::value_desc("inst_id"));

static cl::opt<bool> ProfileGlobals("slamp-profile-globals", cl::init(true),
                                    cl::NotHidden, cl::desc("Profile globals"));

static cl::opt<bool> IgnoreCall("slamp-ignore-call", cl::init(false),
                                cl::NotHidden,
                                cl::desc("Ignore dependences from call"));

// targeting DOALL
static cl::opt<bool> IsDOALL("slamp-doall", cl::init(false), cl::NotHidden,
                             cl::desc("Doall"));

// targeting DSWP
static cl::opt<bool> IsDSWP("slamp-dswp", cl::init(false), cl::NotHidden,
                            cl::desc("DSWP"));

static cl::opt<bool> UsePruning("slamp-pruning", cl::init(false), cl::NotHidden,
                                cl::desc("Use PDG to pruning"));

// target instruction with metadata ID
static cl::opt<uint32_t> TargetInst("slamp-target-inst", cl::init(0),
                                    cl::NotHidden,
                                    cl::desc("Target Instruction"));

cl::opt<std::string> outfile("slamp-outfile", cl::init("result.slamp.profile"),
                             cl::NotHidden, cl::desc("Output file name"));

SLAMP::SLAMP() : ModulePass(ID) {}

SLAMP::~SLAMP() = default;

void SLAMP::getAnalysisUsage(AnalysisUsage &au) const {
  au.addRequired<LoopInfoWrapperPass>();
#ifdef USE_PDG
  au.addRequired<LoopAA>();
  au.addRequired<PDGBuilder>();
#endif
  au.setPreservesAll();
}

static std::vector<uint32_t> elidedLoopInstsId;
// https://stackoverflow.com/questions/20511347/a-good-hash-function-for-a-vector
static size_t elidedHash(std::vector<uint32_t> const &vec) {
  std::size_t seed = vec.size();
  for (auto &i : vec) {
    seed ^= i + 0x9e3779b9 + (seed << 6) + (seed >> 2);
  }
  return seed;
}

uint32_t findMaxInstructionId(Module &m) {
  uint32_t maxInstructionId = 0;
  for (auto &f : m) {
    if (f.isDeclaration())
      continue;
    for (auto &bb : f) {
      for (auto &inst : bb) {
        auto id = Namer::getInstrId(&inst);
        if (id == -1) {
          continue;
        }
        if (id > maxInstructionId) {
          maxInstructionId = id;
        }
      }
    }
  }

  return maxInstructionId;
}

Value *getGlobalName(GlobalVariable *gv) {
  Module *mod = gv->getParent();
  std::string name = "global " + gv->getName().str();
  return getStringLiteralExpression(*mod, name);
}

// copy debug information or create a bogus one
Instruction *updateDebugInfo(Instruction *inserted, Instruction *location,
                             Module &m) {
  if (location->getMetadata(LLVMContext::MD_dbg)) {
    inserted->copyMetadata(*location, ArrayRef<unsigned>{LLVMContext::MD_dbg});
    // inserted->setDebugLoc(location->getDebugLoc());
  } else {
    auto scope = location->getParent()->getParent()->getSubprogram();
    // if scope is empty, create a bogus one
    if (scope == nullptr) {
      errs() << "Warning: no scope for " << *location << ":"
             << location->getParent()->getParent()->getName() << "\n";
      // find main function
      scope = m.getFunction("main")->getSubprogram();
    }
    // if there's no scope for main, then do not add new debug info
    if (scope == nullptr) {
      return inserted;
    }

    DILocation *loc = DILocation::get(m.getContext(), 0, 0, scope);
    inserted->setMetadata(LLVMContext::MD_dbg, loc);
  }

  return inserted;
}

unordered_map<Instruction *, uint32_t> instOffsetMap;

void generateInstOffset(Module &m) {
  // generate instruction offset
  for (auto &f : m) {
    if (f.isDeclaration())
      continue;
    for (auto &bb : f) {
      unsigned offsetWithinBlock = 0;
      for (auto &inst : bb) {
        instOffsetMap[&inst] = offsetWithinBlock++;
      }
    }
  }
}

bool SLAMP::runOnModule(Module &m) {
  // generate instruction offset
  generateInstOffset(m);

  LLVMContext &ctxt = m.getContext();

  // frequently used types
  Void = Type::getVoidTy(ctxt);
  I32 = Type::getInt32Ty(ctxt);
  I64 = Type::getInt64Ty(ctxt);
  I8Ptr = Type::getInt8PtrTy(ctxt);

  // find target function/loop
  if (TargetLoopEnabled && !findTarget(m))
    return false;

  // TODO: might want to check the whole program if no target loop is found
  // check if target may call setjmp/longjmp
  if (TargetLoopEnabled && mayCallSetjmpLongjmp(this->target_loop)) {
    LLVM_DEBUG(errs() << "Warning! target loop may call setjmp/longjmp\n");
    // return false;
  }

#ifdef USE_PDG
  // User set the explicit insts through slamp-explicit-insts
  if (!ExplicitInsts.empty()) {
    for (auto *BB : this->target_loop->blocks()) {
      for (Instruction &I : *BB) {
        if (!I.mayReadOrWriteMemory()) {
          continue;
        }

        auto inst_id = Namer::getInstrId(&I);

        // check if the instruction is explicitly instrumented
        if (std::find(ExplicitInsts.begin(), ExplicitInsts.end(), inst_id) !=
            ExplicitInsts.end()) {
          numInstrumentedNode++;
        } else {
          elidedLoopInstsId.push_back(inst_id);
          elidedLoopInsts.insert(&I);
          numElidedNode++;
        }
      }
    }
  }
  // User set the targeted inst through slamp-target-inst
  else if (TargetInst != 0) {
    auto *aa = getAnalysis<LoopAA>().getTopAA();
    aa->dump();

    // find the instruction based on the metadata
    Instruction *target_inst = nullptr;
    for (auto *BB : this->target_loop->blocks()) {
      for (Instruction &I : *BB) {
        if (!I.mayReadOrWriteMemory()) {
          continue;
        }
        if (Namer::getInstrId(&I) == TargetInst) {
          target_inst = &I;
          break;
        }
      }
    }

    if (target_inst == nullptr) {
      errs() << "Error! cannot find target instruction\n";

      for (auto *BB : this->target_loop->blocks()) {
        for (Instruction &I : *BB) {
          if (!I.mayReadOrWriteMemory()) {
            continue;
          }
          elidedLoopInstsId.push_back(Namer::getInstrId(&I));
          elidedLoopInsts.insert(&I);
          numElidedNode++;
        }
      }
    } else {

      errs() << "Target ID: " << TargetInst << "\n";
      errs() << "Target instruction: " << *target_inst << "\n";

      for (auto *BB : this->target_loop->blocks()) {
        for (Instruction &I : *BB) {
          if (!I.mayReadOrWriteMemory()) {
            continue;
          }
          if (&I == target_inst) {
            numInstrumentedNode++;
            continue;
          }
          // any dependence that has a RAW dep to the target instruction should
          // not be elided
          auto retLCFW = liberty::disproveLoopCarriedMemoryDep(
              target_inst, &I, 0b111, this->target_loop, aa);
          auto retLCBW = liberty::disproveLoopCarriedMemoryDep(
              &I, target_inst, 0b111, this->target_loop, aa);
          auto retIIFW = liberty::disproveIntraIterationMemoryDep(
              target_inst, &I, 0b111, this->target_loop, aa);
          auto retIIBW = liberty::disproveIntraIterationMemoryDep(
              &I, target_inst, 0b111, this->target_loop, aa);

          // debug
          LLVM_DEBUG(if (Namer::getInstrId(&I) == 21182) {
            Remedies remedies;
            LoopAA::ModRefResult modrefIIFW = aa->modref(
                target_inst, LoopAA::Same, &I, target_loop, remedies);
            auto modrefIIBW = aa->modref(&I, LoopAA::Same, target_inst,
                                         target_loop, remedies);
            errs() << "Target inst: " << *target_inst << "\n";
            errs() << "I: " << I << "\n";
            // convert retLCFW to int and print
            errs() << "retLCFW: " << (int)(retLCFW) << "\n";
            errs() << "retLCBW: " << (int)(retLCBW) << "\n";
            errs() << "retIIFW: " << (int)(retIIFW) << "\n";
            errs() << "retIIBW: " << (int)(retIIBW) << "\n";
            errs() << "modrefIIFW: " << (modrefIIFW) << "\n";
            errs() << "modrefIIBW: " << (modrefIIBW) << "\n";
          });

          // RAW disproved for all deps
          if ((retLCFW & 0b001) && (retLCBW & 0b001) && (retIIFW & 0b001) &&
              (retIIBW & 0b001)) {
            elidedLoopInstsId.push_back(Namer::getInstrId(&I));
            elidedLoopInsts.insert(&I);
            numElidedNode++;
          } else {
            numInstrumentedNode++;
          }
        }
      }
    }
  } else if (UsePruning || IsDOALL) { // is DOALL implies use pruning
    // get PDG and prune
    auto *pdgbuilder = getAnalysisIfAvailable<PDGBuilder>();

    if (pdgbuilder) {
      auto pdg = pdgbuilder->getLoopPDG(this->target_loop);
      errs() << "Try to elide nodes " << pdg->numNodes() << "\n";
      // go through all the nodes and see if they still have potential
      // dependences
      for (auto node : pdg->getNodes()) {
        bool canBeElided = true;

        // have to be instruction and mayReadOrWriteMemory
        if (auto inst = dyn_cast<Instruction>(node->getT())) {
          if (!inst->mayReadOrWriteMemory()) {
            continue;
          }
        } else {
          continue;
        }

        numInstrumentedNode++;

        if (dyn_cast<Instruction>(node->getT())->mayWriteToMemory()) {
          for (auto &edge : node->getOutgoingEdges()) {
            if (edge->isRAWDependence() && edge->isMemoryDependence()) {
              // FIXME: hack for DOALL
              if (IsDOALL && !edge->isLoopCarriedDependence()) {
                continue;
              }
              if (IgnoreCall) {
                // ignore dep to call
                if (isa<CallBase>(edge->getIncomingT()) ||
                    isa<CallBase>(edge->getOutgoingT())) {
                  continue;
                }
              }
              canBeElided = false;
              break;
            }
          }
        }

        if (dyn_cast<Instruction>(node->getT())->mayReadFromMemory()) {
          for (auto &edge : node->getIncomingEdges()) {
            if (edge->isRAWDependence() && edge->isMemoryDependence()) {
              // FIXME: hack for DOALL
              if (IsDOALL && !edge->isLoopCarriedDependence()) {
                continue;
              }
              if (IgnoreCall) {
                // ignore dep to call
                if (isa<CallBase>(edge->getOutgoingT()) ||
                    isa<CallBase>(edge->getIncomingT())) {
                  continue;
                }
              }
              canBeElided = false;
              break;
            }
          }
        }

        if (canBeElided) {
          if (auto *inst = dyn_cast<Instruction>(node->getT())) {
            errs() << "Elided: " << *inst << "\n";
            numElidedNode++;
            numInstrumentedNode--;
            elidedLoopInsts.insert(inst);
            elidedLoopInstsId.push_back(Namer::getInstrId(inst));
          }
        }
      }
    } else {
      errs() << "PDGBuilder not added, cannot elide nodes\n";
    }
  } else {
    errs() << "No elision technique is selected\n";
    for (auto *BB : this->target_loop->blocks()) {
      for (Instruction &I : *BB) {
        if (!I.mayReadOrWriteMemory()) {
          continue;
        }
        numInstrumentedNode++;
      }
    }
  }
#endif

  numInstrumentedNodeStats = numInstrumentedNode;
  numElidedNodeStats = numElidedNode;

  errs() << "Instrumented Count: " << numInstrumentedNode << "\n";
  errs() << "Elided Count: " << numElidedNode << "\n";
  std::sort(elidedLoopInstsId.begin(), elidedLoopInstsId.end());
  errs() << "Elided Hash: " << elidedHash(elidedLoopInstsId) << "\n";

  //// replace external function calls to wrapper function calls
  replaceExternalFunctionCalls(m);

  Function *ctor = instrumentConstructor(m);
  instrumentDestructor(m);

  if (ProfileGlobals) {
    instrumentGlobalVars(m, ctor);
  }

  instrumentAllocas(m);
  // instrument all base pointer creation
  instrumentBasePointer(m);

  instrumentFunctionStartStop(m);
  instrumentMainFunction(m);

  if (TargetLoopEnabled) {
    instrumentLoopStartStop(m, this->target_loop);
  }

  if (TargetLoopEnabled) {
    instrumentInstructions(m, this->target_loop);
  } else {
    instrumentInstructions(m);
  }
  instrumentLoopStartStopForAll(m);

  // insert implementations for runtime wrapper functions, which calls the
  // binary standard function
  addWrapperImplementations(m);

  return true;
}

/// Find target function and loop baed on the options passed in
bool SLAMP::findTarget(Module &m) {
  bool found = false;

  for (auto &fi : m) {
    Function *f = &fi;

    if (f->getName().str() == TargetFcn) {
      BasicBlock *header = nullptr;

      for (auto &bi : *f) {
        if (bi.getName().str() == TargetLoop) {
          header = &bi;
          break;
        }
      }

      if (header == nullptr)
        break;

      // FIXME: dangerous, this loop pointer may not persist
      LoopInfo &loopinfo = getAnalysis<LoopInfoWrapperPass>(*f).getLoopInfo();
      this->target_loop = loopinfo.getLoopFor(header);

      if (!this->target_loop)
        break;

      this->target_fn = f;
      found = true;
    }
  }

  return found;
}

static bool is_setjmp_or_longjmp(Function *f) {
  string name = f->getName().str();
  if (name == "_setjmp" || name == "longjmp")
    return true;
  else
    return false;
}

bool SLAMP::mayCallSetjmpLongjmp(Loop *loop) {
  set<Function *> callables;
  getCallableFunctions(loop, callables);

  return (find_if(callables.begin(), callables.end(), is_setjmp_or_longjmp) !=
          callables.end());
}

void SLAMP::getCallableFunctions(Loop *loop, set<Function *> &callables) {
  for (auto &bb : loop->getBlocks()) {
    for (auto &ii : *bb) {
      // FIXME: not only callinst are callable
      auto *ci = dyn_cast<CallInst>(&ii);
      if (!ci)
        continue;
      getCallableFunctions(ci, callables);
    }
  }
}

void SLAMP::getCallableFunctions(CallInst *ci, set<Function *> &callables) {
  Function *called_fn = ci->getCalledFunction();
  if (called_fn == nullptr) {
    // analyze indirect function call */
    set<Function *> targets;

    // get functions callable by given callinst
    getFunctionsWithSign(ci, targets);

    // check matched functions
    for (auto target : targets) {
      if (callables.find(target) == callables.end()) {
        callables.insert(target);
        getCallableFunctions(target, callables);
      }
    }
  } else {
    if (callables.find(called_fn) == callables.end()) {
      callables.insert(called_fn);
      getCallableFunctions(called_fn, callables);
    }
  }
}

void SLAMP::getCallableFunctions(Function *f, set<Function *> &callables) {
  for (inst_iterator ii = inst_begin(f); ii != inst_end(f); ii++) {
    // FIXME: not only callinst are callable
    auto *ci = dyn_cast<CallInst>(&*ii);
    if (!ci)
      continue;
    getCallableFunctions(ci, callables);
  }
}

void SLAMP::getFunctionsWithSign(CallInst *ci, set<Function *> matched) {
  Module *m = ci->getParent()->getParent()->getParent();
  CallBase &cs = cast<CallBase>(*ci);

  for (auto &fi : *m) {
    Function *func = &fi;

    bool found = true;
    // compare signature
    if (func->isVarArg()) {
      if (func->arg_size() > cs.arg_size())
        found = false;
    } else {
      if (func->arg_size() != cs.arg_size())
        found = false;
    }

    if (found) {
      Function::arg_iterator fai;
      User::op_iterator cai;
      for (fai = func->arg_begin(), cai = cs.arg_begin();
           fai != func->arg_end(); fai++, cai++) {
        Value *af = &*fai;
        Value *ac = *cai;
        if (af->getType() != ac->getType()) {
          found = false;
          break;
        }
      }
    }

    if (found)
      matched.insert(func);
  }
}

std::string getInstructionName(Instruction *inst) {
  auto fcn = inst->getParent()->getParent();
  auto bb = inst->getParent();

  std::stringstream sout;
  sout << fcn->getName().str() << ' ' << bb->getName().str() << ' ';

  if (inst->hasName())
    sout << inst->getName().str();
  else {
    // find the offset within the block
    sout << '$' << instOffsetMap[inst];
  }

  return sout.str();
}

std::string getArgName(Argument *arg) {
  Function *fcn = arg->getParent();

  std::ostringstream name;
  name << "argument " << fcn->getName().str() << " %" << arg->getArgNo();
  return name.str();
}

// Replace external functions with SLAMP prefixed ones (SLAMP_xxx)
// The list of SLAMP functions are given in `externs.h`
void SLAMP::replaceExternalFunctionCalls(Module &m) {
  // initialize a set of external function names
  auto *push = cast<Function>(
      m.getOrInsertFunction("SLAMP_ext_push", Void, I32).getCallee());
  auto *pop =
      cast<Function>(m.getOrInsertFunction("SLAMP_ext_pop", Void).getCallee());

  set<string> externs;
  for (unsigned i = 0, e = sizeof(externs_str) / sizeof(externs_str[0]); i < e;
       i++)
    externs.insert(externs_str[i]);

  // initialize a set of external functions not to be implemented
  set<string> ignores;
  for (unsigned i = 0,
                e = sizeof(ignore_externs_str) / sizeof(ignore_externs_str[0]);
       i < e; i++)
    ignores.insert(ignore_externs_str[i]);

  vector<Function *> funcs;

  for (auto &fi : m) {
    Function *func = &fi;

    // only external functions are of interest
    if (!func->isDeclaration())
      continue;

    // filter functions to ignore
    if (ignores.find(func->getName().str()) != ignores.end())
      continue;

    // FIXME: malloc can be an intrinsic function, not all intrinsics can be
    // ignored
    if (func->isIntrinsic()) {
      // just confirm that all uses is an intrinsic instruction
      for (Value::user_iterator ui = func->user_begin(); ui != func->user_end();
           ui++)
        assert(isa<IntrinsicInst>(*ui));
      continue;
    }

    funcs.push_back(func);
  }

  bool hasUnrecognizedFunction = false;
  for (auto func : funcs) {
    string name = func->getName().str();

    // start with SLAMP_, ignore it
    if (name.find("SLAMP_") == 0) {
      continue;
    }

    // find all usage of the function
    // add a slamp_push and slamp_pop around it
    for (auto user : func->users()) {
      // get instruction
      auto *inst = dyn_cast<Instruction>(user);
      if (inst == nullptr)
        continue;

      // make sure it's a call to the function
      if (!isa<CallBase>(inst))
        continue;
      auto *cb = dyn_cast<CallBase>(inst);
      if (cb->getCalledFunction() != func)
        continue;

      // FIXME: duplicated code as instrumentLoopInst
      auto id = Namer::getInstrId(inst);
      if (id == -1) {
        continue;
      }
      vector<Value *> args;
      args.push_back(ConstantInt::get(I32, id));
      InstInsertPt pt = InstInsertPt::Before(inst);
      pt << updateDebugInfo(CallInst::Create(push, args), pt.getPosition(), m);

      errs() << "Malloc ID " << id << " : " << getInstructionName(inst) << "\n";

      if (isa<CallInst>(inst)) {
        pt = InstInsertPt::After(inst);
        pt << updateDebugInfo(CallInst::Create(pop), pt.getPosition(), m);
      } else if (auto *invokeI = dyn_cast<InvokeInst>(inst)) {
        // for invoke, need to find the two paths and add pop
        auto insertPop = [&pop, &m](BasicBlock *entry) {
          InstInsertPt pt;
          if (isa<LandingPadInst>(entry->getFirstNonPHI()))
            pt = InstInsertPt::After(entry->getFirstNonPHI());
          else
            pt = InstInsertPt::Before(entry->getFirstNonPHI());

          pt << updateDebugInfo(CallInst::Create(pop), pt.getPosition(), m);
        };

        insertPop(invokeI->getNormalDest());
        // FIXME: will generate mulitiple `slamp_pop` after the landing pad
        //        Fine for now because `slamp_pop` only set the context to 0
        insertPop(invokeI->getUnwindDest());

      } else {
        assert(false && "Call but not CallInst nor InvokeInst");
      }
    }

    //// FIXME: temporarily turn off the replacement
    // if (externs.find(name) == externs.end()) {
    //   // check if the function argument is `readnone`, then it's pure
    //   if (func->hasFnAttribute(llvm::Attribute::AttrKind::ReadNone)) {
    //     continue;
    //   }

    //   errs() << "WARNING: Wrapper for external function " << name
    //     << " not implemented.\n";
    //   hasUnrecognizedFunction = true;

    // } else {
    //   string wrapper_name = "SLAMP_" + name;
    /* Function* wrapper = cast<Function>( m.getOrInsertFunction(wrapper_name,
     * func->getFunctionType() ) ); */
    //   FunctionCallee wrapper =
    //     m.getOrInsertFunction(wrapper_name, func->getFunctionType());

    //   // replace 'func' to 'wrapper' in uses
    //   func->replaceAllUsesWith(wrapper.getCallee());
    // }
  }

  if (hasUnrecognizedFunction) {
    // assert only turned on for debug
    // assert(false && "Wrapper for external function not implemented.\n");
    LLVM_DEBUG(errs() << "Wrapper for external function not implemented.\n");
  }
}

/// Create a function `___SLAMP_ctor` that calls `SLAMP_init` and
/// `SLAMP_init_global_vars` before everything (llvm.global_ctors)
Function *SLAMP::instrumentConstructor(Module &m) {
  LLVMContext &c = m.getContext();
  auto *ctor =
      cast<Function>(m.getOrInsertFunction("___SLAMP_ctor", Void).getCallee());
  BasicBlock *entry = BasicBlock::Create(c, "entry", ctor, nullptr);
  ReturnInst::Create(c, entry);
  callBeforeMain(ctor, 65534);

  // call SLAMP_init function

  // Function* init = cast<Function>( m.getOrInsertFunction( "SLAMP_init", Void,
  // I32, I32, (Type*)0) );
  auto *init = cast<Function>(
      m.getOrInsertFunction("SLAMP_init", Void, I32, I32, I32).getCallee());

  uint32_t maxInstructionId = findMaxInstructionId(m);
  uint32_t targetFunctionId = 0;
  uint32_t targetLoopId = 0;
  if (TargetLoopEnabled) {
    targetFunctionId = Namer::getFuncId(this->target_fn);
    targetLoopId = Namer::getBlkId(this->target_loop->getHeader());
  }
  Value *args[] = {ConstantInt::get(I32, maxInstructionId),
                   ConstantInt::get(I32, targetFunctionId),
                   ConstantInt::get(I32, targetLoopId)};
  CallInst::Create(init, args, "", entry->getTerminator());

  return ctor;
}

/// Create a function `___SLAMP_dtor` that calls `SLAMP_fini`, register through
/// `llvm.global_dtors`
void SLAMP::instrumentDestructor(Module &m) {
  LLVMContext &c = m.getContext();
  auto *dtor =
      cast<Function>(m.getOrInsertFunction("___SLAMP_dtor", Void).getCallee());
  BasicBlock *entry = BasicBlock::Create(c, "entry", dtor, nullptr);
  ReturnInst::Create(c, entry);
  callAfterMain(dtor, 65534);

  // call SLAMP_fini function
  auto *fini = cast<Function>(
      m.getOrInsertFunction("SLAMP_fini", Void, I8Ptr).getCallee());
  Constant *filename = getStringLiteralExpression(m, outfile);
  Value *args[] = {filename};

  CallInst::Create(fini, args, "", entry->getTerminator());
}

/// Go through all global variables and call `SLAMP_init_global_vars`
void SLAMP::instrumentGlobalVars(Module &m, Function *ctor) {
  // DataLayout& td = getAnalysis<DataLayout>();
  const DataLayout &td = m.getDataLayout();
  BasicBlock *entry = &(ctor->getEntryBlock());

  // call SLAMP_init_global_vars function to initialize shadow memory for
  // global variables
  auto *init_gvars = cast<Function>(
      m.getOrInsertFunction("SLAMP_init_global_vars", Void, I8Ptr, I64, I64)
          .getCallee());

  for (GlobalVariable &gvr : m.globals()) {
    GlobalVariable *gv = &gvr;

    if (gv->getName() == "llvm.global_ctors") // explicitly skip global ctor
      continue;
    else if (gv->getName() ==
             "llvm.global_dtors") // explicitly skip global dtor
      continue;

    auto *ty = dyn_cast<PointerType>(gv->getType());
    assert(ty);

    InstInsertPt pt = InstInsertPt::Before(entry->getTerminator());
    // get name of the global

    // Value *name = getGlobalName(gv);
    // FIXME: rename global variable to its name
    Value *name = getStringLiteralExpression(m, "hello");
    uint64_t size = td.getTypeStoreSize(ty->getElementType());
    Value *args[] = {name, castToInt64Ty(gv, pt), ConstantInt::get(I64, size)};
    pt << CallInst::Create(init_gvars, args);
  }

  for (auto &&fi : m) {
    Function *func = &fi;

    if (func->isIntrinsic())
      continue;

    uint64_t size = td.getTypeStoreSize(func->getType());

    // FIXME: rename global variable to its name
    Value *name = getStringLiteralExpression(m, "hello");
    InstInsertPt pt = InstInsertPt::Before(entry->getTerminator());
    Value *args[] = {name, castToInt64Ty(func, pt),
                     ConstantInt::get(I64, size)};
    pt << CallInst::Create(init_gvars, args);
  }
}

void SLAMP::findLifetimeMarkers(Value *i, set<const Value *> &already,
                                std::vector<Instruction *> &starts,
                                std::vector<Instruction *> &ends) {
  if (already.count(i))
    return;
  already.insert(i);

  for (Value::user_iterator inst = i->user_begin(), end = i->user_end();
       inst != end; ++inst) {
    User *user = &**inst;

    if (BitCastInst *cast = dyn_cast<BitCastInst>(user))
      findLifetimeMarkers(cast, already, starts, ends);

    else if (IntrinsicInst *intrin = dyn_cast<IntrinsicInst>(user)) {
      if (intrin->getIntrinsicID() == Intrinsic::lifetime_start)
        starts.push_back(intrin);
      else if (intrin->getIntrinsicID() == Intrinsic::lifetime_end)
        ends.push_back(intrin);
    }
  }
}

void SLAMP::reportStartOfAllocaLifetime(AllocaInst *inst, Instruction *start,
                                        Function *fcn, const DataLayout &dl,
                                        Module &m) {

  IRBuilder<> Builder(start->getNextNode());
  // input of callback function
  // TODO: get alloca size: current function gets number of allocation items
  Value *array_sz = inst->getArraySize();
  if (array_sz->getType() != I64)
    array_sz = Builder.CreateIntCast(array_sz, I64, false);

  auto type_sz = dl.getTypeStoreSize(inst->getAllocatedType());
  // get address of allocaa by executina allocainst
  Value *addr = static_cast<Value *>(inst);
  // Instruction *ptrcast = CastInst::CreatePointerCast(addr, I64);
  // Value* return_addr = Builder.CreatePointerCast(addr, I64);
  Value *ptrcast = Builder.CreatePointerCast(addr, I64);

  // get instruction ID of lifetime start
  // Value *start_value = Namer::getInstrId(start);

  Type *ptype[4] = {I32, I64, I64, I64};

  vector<Value *> args;
  args.push_back(ConstantInt::get(I32, Namer::getInstrId(start)));
  args.push_back(ptrcast);
  args.push_back(array_sz);
  args.push_back(ConstantInt::get(I64, type_sz));

  FunctionType *fty = FunctionType::get(Void, ptype, false);

  CallInst *alloca_start_call = Builder.CreateCall(fty, fcn, args);

  updateDebugInfo(alloca_start_call, start, m);

  return;
}

void SLAMP::reportEndOfAllocaLifetime(AllocaInst *inst, Instruction *end,
                                      bool empty, Function *fcn, Module &m) {
  // Value *params[] = {};
  // Type *params_type[] = {};

  if (!empty) {
    IRBuilder<> Builder(end);

    Value *addr = static_cast<Value *>(inst);
    // Instruction *ptrcast = CastInst::CreatePointerCast(addr, I64);
    // Value* return_addr = Builder.CreatePointerCast(addr, I64);
    Value *ptrcast = Builder.CreatePointerCast(addr, I64);
    Type *ptype[2] = {I32, I64};

    vector<Value *> args;
    args.push_back(ConstantInt::get(I32, Namer::getInstrId(end)));
    args.push_back(ptrcast);
    FunctionType *fty = FunctionType::get(Void, ptype, false);
    CallInst *alloca_end_call = Builder.CreateCall(fty, fcn, args);
    updateDebugInfo(alloca_end_call, end, m);
  } else {
    // TODO:search for terminator block
    auto *F = inst->getFunction();

    // find all return instructions
    // IRBuilder<> Builder();
  }
  return;
}

/// For each pointer use in the targeted loop
/// 1. find the base pointer
/// 2. Go to the creation time of the pointer
/// 3. Insert a call to SLAMP_report_base_pointer(instruction, address)
void SLAMP::instrumentBasePointer(Module &m) {

  const DataLayout &DL = m.getDataLayout();

  auto *find_underlying_arg =
      cast<Function>(m.getOrInsertFunction("SLAMP_report_base_pointer_arg",
                                           Void, I32, I32, I8Ptr)
                         .getCallee());
  auto *find_underlying_inst = cast<Function>(
      m.getOrInsertFunction("SLAMP_report_base_pointer_inst", Void, I32, I8Ptr)
          .getCallee());

  // collect all pointer use by load, store, function argument in the targeted
  // loop
  std::set<const Value *> indeterminate_pointers, indeterminate_objects,
      already;

  for (auto &F : m) {
    for (auto &BB : F) {
      SpecPriv::Indeterminate::findIndeterminateObjects(
          BB, indeterminate_pointers, indeterminate_objects);
    }
  }

  for (auto &object : indeterminate_objects) {
    if (const auto *const_arg = dyn_cast<Argument>(object)) {
      if (already.count(const_arg))
        continue;
      already.insert(const_arg);

      LLVM_DEBUG(
          errs()
          << "Instrumenting indeterminate base object in function argument "
          << *const_arg << "\n");

      auto *arg = const_cast<Argument *>(const_arg);
      Function *fcn = arg->getParent();

      Instruction *cast = new BitCastInst(arg, I8Ptr);
      auto fcnId = Namer::getFuncId(fcn);
      auto argId = arg->getArgNo();

      auto pt = InstInsertPt::Beginning(fcn);
      Value *args[] = {ConstantInt::get(I32, fcnId),
                       ConstantInt::get(I32, argId), cast};
      pt << cast
         << updateDebugInfo(CallInst::Create(find_underlying_arg, args),
                            pt.getPosition(), m);

      // errs() << "UO Arg (" << fcnId << "," << argId <<  ") : "
      errs() << "UO Arg " << (fcnId << 5 | ((0x1f & (argId << 4)) | 0x1))
             << " : " << getArgName(arg) << "\n";
    } else if (const auto *const_inst = dyn_cast<Instruction>(object)) {
      if (already.count(const_inst))
        continue;
      already.insert(const_inst);

      if (isa<AllocaInst>(const_inst))
        continue;

      LLVM_DEBUG(errs() << "Instrumenting indeterminate base object: "
                        << *const_inst << '\n');
      auto *inst = const_cast<Instruction *>(const_inst);

      Instruction *cast = new BitCastInst(inst, I8Ptr);

      InstInsertPt where;
      if (auto *invoke = dyn_cast<InvokeInst>(inst)) {
        auto entry = invoke->getNormalDest();
        if (isa<LandingPadInst>(entry->getFirstNonPHI())) {
          where = InstInsertPt::After(entry->getFirstNonPHI());
        } else {
          // iterate all the phi node in the entry block
          // if inst is one of the incoming value of the phi node
          // then convert the cast to the phi node
          for (auto &phi : entry->phis()) {
            bool found = false;
            for (auto &incoming : phi.incoming_values()) {
              if (incoming == inst) {
                // replace the cast with the phi node
                cast->deleteValue();
                cast = new BitCastInst(&phi, I8Ptr);
                found = true;
                break;
              }
            }
            if (found)
              break;
          }
          where = InstInsertPt::Before(entry->getFirstNonPHI());
        }
      } else if (auto *phi = dyn_cast<PHINode>(inst)) {
        // Don't accidentally insert instrumentation before
        // later PHIs or landing pad instructions.
        where = InstInsertPt::Beginning(phi->getParent());
      } else
        where = InstInsertPt::After(inst);

      auto instId = Namer::getInstrId(inst);
      Value *args[] = {ConstantInt::get(I32, instId), cast};
      where << cast
            << updateDebugInfo(CallInst::Create(find_underlying_inst, args),
                               where.getPosition(), m);

      errs() << "UO Inst " << (instId << 1 | 0x0) << " : "
             << getInstructionName(inst) << "\n";
    } else {
      errs() << "What is: " << *object << '\n';
      assert(false && "Unknown object type?!?!");
    }
  }
}

// For each alloca, find the lifetime starts and ends
// and insert calls to `SLAMP_callback_stack_alloca` and
// `SLAMP_callback_stack_free`
void SLAMP::instrumentAllocas(Module &m) {
  const DataLayout &dl = m.getDataLayout();

  typedef std::vector<Instruction *> IList;
  typedef std::set<const Value *> ValSet;
  // List of instructions with allocas
  std::vector<AllocaInst *> allocas;

  auto *stack_alloca_fcn =
      cast<Function>(m.getOrInsertFunction("SLAMP_callback_stack_alloca", Void,
                                           I32, I64, I64, I64)
                         .getCallee());
  auto *stack_free_fcn = cast<Function>(
      m.getOrInsertFunction("SLAMP_callback_stack_free", Void, I32, I64)
          .getCallee());

  // Collect all alloca instructions
  for (auto &f : m) {
    Function *F = &f;
    for (inst_iterator i = inst_begin(F), e = inst_end(F); i != e; i++) {
      Instruction *inst = &*i;
      if (isa<AllocaInst>(inst)) {
        AllocaInst *alloca_inst = cast<AllocaInst>(inst);
        allocas.push_back(alloca_inst);
      }
    }
  }
  Type *ptype[4] = {I64, I64, I32, I64};
  FunctionType *fty = FunctionType::get(Void, ptype, false);

  for (auto i : allocas) {
    // Find explicit lifetime markers
    IList starts, ends;
    ValSet avoidInfiniteRecursion;
    findLifetimeMarkers(i, avoidInfiniteRecursion, starts, ends);

    if (starts.empty())
      starts.push_back(i);
    // Report start of lifetime
    for (unsigned k = 0, N = starts.size(); k < N; k++)
      reportStartOfAllocaLifetime(i, starts[k], stack_alloca_fcn, dl, m);

    // FIXME: how do we define end-of-fucntion?
    // if (ends.empty())
    //   reportEndOfAllocaLifetime(i, NULL, 1, stack_free_fcn);
    // Report end of lifetime

    for (unsigned k = 0, N = ends.size(); k < N; k++)
      reportEndOfAllocaLifetime(i, ends[k], 0, stack_free_fcn, m);
  }
}

// /// FIXME: not called anywhere
// void SLAMP::instrumentNonStandards(Module &m, Function *ctor) {
//   // 1) handle __errno_location.
//   allocErrnoLocation(m, ctor);
// }

// /// FIXME: not clear what this do
// void SLAMP::allocErrnoLocation(Module &m, Function *ctor) {
//   // DataLayout&  td = getAnalysis<DataLayout>();
//   const DataLayout &td = m.getDataLayout();
//   LLVMContext &c = m.getContext();

//   // Call dummy __errno_location to allocate a shadow memory for the location
//   auto *f = cast<Function>(
//       m.getOrInsertFunction("SLAMP___errno_location_alloc",
//       Void).getCallee());

//   BasicBlock *entry = BasicBlock::Create(c, "entry", f, nullptr);
//   auto *c0 = cast<Function>(
//       m.getOrInsertFunction("__errno_location", I32->getPointerTo())
//           .getCallee());
//   InstInsertPt pt = InstInsertPt::Beginning(entry);
//   CallInst *ci = CallInst::Create(c0, "");
//   pt << ci;

//   // ci is a __errno_location call
//   auto *ty = dyn_cast<PointerType>(ci->getType());
//   assert(ty);
//   uint64_t size = td.getTypeStoreSize(ty->getElementType());

//   // reuse SLAMP_init_global_vars
//   auto *init_gvars = dyn_cast<Function>(
//       m.getOrInsertFunction("SLAMP_init_global_vars", Void, I64, I64)
//           .getCallee());
//   assert(init_gvars);

//   Value *args[] = {castToInt64Ty(ci, pt), ConstantInt::get(I64, size)};
//   CallInst *ci2 = CallInst::Create(init_gvars, args, "");
//   pt << ci2;

//   pt << ReturnInst::Create(c);

//   // call function f from ctor
//   BasicBlock *ctor_entry = &(ctor->getEntryBlock());
//   CallInst::Create(f, "", ctor_entry->getTerminator());
// }

/// Add SLAMP_main_entry as the first thing in main
void SLAMP::instrumentMainFunction(Module &m) {
  for (auto &fi : m) {
    Function *func = &fi;
    if (func->getName() != "main")
      continue;

    BasicBlock *entry = &(func->getEntryBlock());

    // if the function is a main function, add special instrumentation to handle
    // command line arguments
    auto *f_main_entry = cast<Function>(
        m.getOrInsertFunction("SLAMP_main_entry", Void, I32,
                              I8Ptr->getPointerTo(), I8Ptr->getPointerTo())
            .getCallee());

    vector<Value *> main_args;
    for (auto &&ai : func->args())
      main_args.push_back(&ai);

    // make up arguments
    if (main_args.size() != 3) { // if not all of argc, argv, evn are given
      Value *zeroarg = ConstantInt::get(I32, 0);
      Value *nullarg = ConstantPointerNull::get(I8Ptr->getPointerTo());

      if (main_args.size() == 0) {        // no command line input
        main_args.push_back(zeroarg);     // argc
        main_args.push_back(nullarg);     // argv
        main_args.push_back(nullarg);     // envp
      } else if (main_args.size() == 2) { // only argc, argv given
        main_args.push_back(nullarg);
      } else {
        assert(false && "Only have one argument (argc) in main, do not conform "
                        "to standard");
      }
    }

    InstInsertPt pt;
    if (isa<LandingPadInst>(entry->getFirstNonPHI()))
      pt = InstInsertPt::After(entry->getFirstNonPHI());
    else
      pt = InstInsertPt::Before(entry->getFirstNonPHI());

    // // read rsp and push it into main_args

    // FunctionType *fty = FunctionType::get(I64, false);
    // // get the static pointer %rsp
    // InlineAsm *get_rsp = InlineAsm::get(
    //     fty, "mov %rsp, $0;", "=r,~{dirflag},~{fpsr},~{flags}", false);

    // CallInst *rsp = CallInst::Create(get_rsp, "");

    pt << updateDebugInfo(CallInst::Create(f_main_entry, main_args, ""),
                          pt.getPosition(), m);
  }
}

/// Pass in the loop and instrument enter/exit hooks
void SLAMP::instrumentLoopStartStopForAll(Module &m) {

  // for all functions
  for (auto &f : m) {
    if (f.isDeclaration())
      continue;
    // get all loops
    LoopInfo &li = getAnalysis<LoopInfoWrapperPass>(f).getLoopInfo();
    for (auto &loop : li.getLoopsInPreorder()) {

      // TODO: check setjmp/longjmp
      BasicBlock *header = loop->getHeader();
      auto loopId = Namer::getBlkId(header);
      if (loopId == -1) {
        assert(false && "Loop header has no id");
      }
      BasicBlock *latch = loop->getLoopLatch();
      vector<Value *> args;
      args.push_back(ConstantInt::get(I32, loopId));

      errs() << "Loop ID " << loopId << " : " << f.getName() << " "
             << header->getName() << " " << li.getLoopDepth(header) << "\n";

      // check if loop-simplify pass executed
      assert(loop->getNumBackEdges() == 1 &&
             "Should be only 1 back edge, loop-simplify?");
      assert(latch && "Loop latch needs to exist, loop-simplify?");

      // add instrumentation on loop header:
      // if new invocation, call SLAMP_loop_invocation, else, call
      // SLAMP_loop_iteration
      FunctionType *fty = FunctionType::get(Void, I32, false);
      auto *f_loop_invoke = cast<Function>(
          m.getOrInsertFunction("SLAMP_enter_loop", fty).getCallee());
      auto *f_loop_iter = cast<Function>(
          m.getOrInsertFunction("SLAMP_loop_iter_ctx", fty).getCallee());
      auto *f_loop_exit = cast<Function>(
          m.getOrInsertFunction("SLAMP_exit_loop", fty).getCallee());

      PHINode *funcphi =
          PHINode::Create(f_loop_invoke->getType(), 2, "funcphi_loop_context");
      InstInsertPt pt;

      if (isa<LandingPadInst>(header->getFirstNonPHI()))
        pt = InstInsertPt::After(header->getFirstNonPHI());
      else
        pt = InstInsertPt::Before(header->getFirstNonPHI());

      pt << funcphi;

      // choose which function to execute (iter or invoke)
      for (auto pred : predecessors(header)) {
        if (pred == latch)
          funcphi->addIncoming(f_loop_iter, pred);
        else
          funcphi->addIncoming(f_loop_invoke, pred);
      }

      updateDebugInfo(
          CallInst::Create(fty, funcphi, args, "", header->getFirstNonPHI()),
          header->getFirstNonPHI(), m);

      // Add `SLAMP_loop_exit` to all loop exits
      SmallVector<BasicBlock *, 8> exits;
      loop->getExitBlocks(exits);

      // one instrumentation per block
      set<BasicBlock *> s;

      for (unsigned i = 0; i < exits.size(); i++) {
        if (s.count(exits[i]))
          continue;

        CallInst *ci = CallInst::Create(f_loop_exit, args);

        InstInsertPt pt2;
        if (isa<LandingPadInst>(exits[i]->getFirstNonPHI()))
          pt2 = InstInsertPt::After(exits[i]->getFirstNonPHI());
        else
          pt2 = InstInsertPt::Before(exits[i]->getFirstNonPHI());

        pt2 << updateDebugInfo(ci, pt2.getPosition(), m);

        s.insert(exits[i]);
      }
    }
  }
}

/// Instrumnent each function entry and exit with SLAMP function entry and exit
/// calls
void SLAMP::instrumentFunctionStartStop(Module &m) {
  // for each function body
  for (auto &fi : m) {
    Function *func = &fi;
    if (func->isDeclaration())
      continue;

    // ignore all SLAMP calls
    if (func->getName().startswith("SLAMP_"))
      continue;

    // find function ID
    auto fcnID = Namer::getFuncId(func);
    if (fcnID == -1) {
      errs() << "Cannot find function ID for " << func->getName() << "\n";
      continue;
    }

    errs() << "Function ID " << fcnID << " : " << func->getName() << "\n";

    // set parameters
    vector<Value *> args;
    args.push_back(ConstantInt::get(I32, fcnID));

    // find the function entry
    auto *f_function_entry = cast<Function>(
        m.getOrInsertFunction("SLAMP_enter_fcn", Void, I32).getCallee());
    auto *f_function_exit = cast<Function>(
        m.getOrInsertFunction("SLAMP_exit_fcn", Void, I32).getCallee());

    // insert SLAMP_enter_fcn at the beginning of the function
    BasicBlock *entry = &(func->getEntryBlock());
    InstInsertPt pt = InstInsertPt::Before(entry->getFirstNonPHI());
    pt << updateDebugInfo(CallInst::Create(f_function_entry, args),
                          pt.getPosition(), m);

    // find all exits of the function
    vector<Instruction *> exits;
    for (auto &bi : *func) {
      BasicBlock *bb = &bi;
      if (isa<ReturnInst>(bb->getTerminator()))
        exits.push_back(bb->getTerminator());
      // else if (isa<ResumeInst>(bb->getTerminator()))
      // exits.push_back(bb->getTerminator());
      // // FIXME: should be at the beginning of the block
      // else if (isa<UnreachableInst>(bb->getTerminator()))
      // exits.push_back(bb->getTerminator());
      //// FIXME: invoke the exception end
      // else if (isa<InvokeInst>(bb->getTerminator()))
      // exits.push_back(bb->getTerminator());
    }

    // insert SLAMP_exit_fcn at the end of the function
    for (auto &exit : exits) {
      InstInsertPt pt = InstInsertPt::Before(exit);
      pt << updateDebugInfo(CallInst::Create(f_function_exit, args),
                            pt.getPosition(), m);
    }
  }
}

/// Pass in the loop and instrument invocation/iteration/exit hooks
void SLAMP::instrumentLoopStartStop(Module &m, Loop *loop) {
  // TODO: check setjmp/longjmp

  BasicBlock *header = loop->getHeader();
  BasicBlock *latch = loop->getLoopLatch();

  // check if loop-simplify pass executed
  assert(loop->getNumBackEdges() == 1 &&
         "Should be only 1 back edge, loop-simplify?");
  assert(latch && "Loop latch needs to exist, loop-simplify?");

  // add instrumentation on loop header:
  // if new invocation, call SLAMP_loop_invocation, else, call
  // SLAMP_loop_iteration
  FunctionType *fty = FunctionType::get(Void, false);
  auto *f_loop_invoke = cast<Function>(
      m.getOrInsertFunction("SLAMP_loop_invocation", fty).getCallee());
  auto *f_loop_iter = cast<Function>(
      m.getOrInsertFunction("SLAMP_loop_iteration", fty).getCallee());
  auto *f_loop_exit =
      cast<Function>(m.getOrInsertFunction("SLAMP_loop_exit", fty).getCallee());

  PHINode *funcphi = PHINode::Create(f_loop_invoke->getType(), 2, "funcphi");
  InstInsertPt pt;

  if (isa<LandingPadInst>(header->getFirstNonPHI()))
    pt = InstInsertPt::After(header->getFirstNonPHI());
  else
    pt = InstInsertPt::Before(header->getFirstNonPHI());

  pt << funcphi;

  // choose which function to execute (iter or invoke)
  for (auto pred : predecessors(header)) {
    if (pred == latch)
      funcphi->addIncoming(f_loop_iter, pred);
    else
      funcphi->addIncoming(f_loop_invoke, pred);
  }

  updateDebugInfo(CallInst::Create(fty, funcphi, "", header->getFirstNonPHI()),
                  header->getFirstNonPHI(), m);

  // Add `SLAMP_loop_exit` to all loop exits
  SmallVector<BasicBlock *, 8> exits;
  loop->getExitBlocks(exits);

  // one instrumentation per block
  set<BasicBlock *> s;

  for (unsigned i = 0; i < exits.size(); i++) {
    if (s.count(exits[i]))
      continue;

    CallInst *ci = CallInst::Create(f_loop_exit, "");

    InstInsertPt pt2;
    if (isa<LandingPadInst>(exits[i]->getFirstNonPHI()))
      pt2 = InstInsertPt::After(exits[i]->getFirstNonPHI());
    else
      pt2 = InstInsertPt::Before(exits[i]->getFirstNonPHI());

    pt2 << updateDebugInfo(ci, pt2.getPosition(), m);

    s.insert(exits[i]);
  }
}

/// Instrument all instructions in a loop
void SLAMP::instrumentInstructions(Module &m, Loop *loop) {
  // collect loop instructions
  set<Instruction *> loopinsts;

  if (TargetLoopEnabled) {
    for (auto &bb : loop->getBlocks())
      for (auto &ii : *bb)
        loopinsts.insert(&ii);
  }

  // go over all instructions in the module
  // - change some intrinsics functions
  // - for instructions within the loop, replace it with normal load/store
  // - for instructions outside of the loop, replace it with external load/store
  for (auto &&f : m) {
    if (f.isDeclaration())
      continue;

    for (auto &&inst : instructions(f)) {
      //// FIXME: ignore lifetime_start/end instrumentation
      // if (const auto Intrinsic = dyn_cast<IntrinsicInst>(&inst)) {
      //   const auto Id = Intrinsic->getIntrinsicID();
      //   if (Id == Intrinsic::lifetime_start || Id == Intrinsic::lifetime_end)
      //   {
      //     instrumentLifetimeIntrinsics(m, &inst);
      //     continue;
      //   }
      // }

      if (auto *mi = dyn_cast<MemIntrinsic>(&inst)) {
        instrumentMemIntrinsics(m, mi);
        continue;
      }
      auto id = Namer::getInstrId(&inst);
      if (id == -1)
        continue; // it's an instrumented instruction, skip

      // if not target loop or found in loopinsts
      if (!TargetLoopEnabled || loopinsts.find(&inst) != loopinsts.end()) {
        instrumentLoopInst(m, &inst, id);
      } else {
        instrumentExtInst(m, &inst, id);
      }
    }
  }
}

/// get the size of the load or store instruction based on the type
int SLAMP::getIndex(PointerType *ty, size_t &size, const DataLayout &DL) {
  int i = DL.getTypeStoreSizeInBits(ty->getElementType());

  // sot: cannot convert a vector value to an int64 so just return variable size
  // n (index 4) and return the actual size, even if i is less than or equal
  // to 64.
  if (isa<VectorType>(ty->getElementType())) {
    size = i / 8;
    return 4;
  }

  switch (i) {
  case 8:
    return 0;
  case 16:
    return 1;
  case 32:
    return 2;
  case 64:
    return 3;
  default:
    size = i / 8;
    return 4;
  }
}

/// handle LLVM Memory intrinsics (memmove, memcpy, memset)
/// This does not replace the function, just add an additional call
// FIXME: is this a complete list?
void SLAMP::instrumentMemIntrinsics(Module &m, MemIntrinsic *mi) {
  CallBase &cb = cast<CallBase>(*mi);
  const Function *callee = cb.getCalledFunction();
  assert(callee);
  string callee_name = callee->getName().str();

  // add intrinsic handlers

  Type *mi_param_types_a[] = {I8Ptr, I8Ptr, I32};
  Type *mi_param_types_b[] = {I8Ptr, I8Ptr, I64};
  Type *mi_param_types_c[] = {I8Ptr, I32};
  Type *mi_param_types_d[] = {I8Ptr, I64};

  FunctionType *mi_fty_a = FunctionType::get(Void, mi_param_types_a, false);
  FunctionType *mi_fty_b = FunctionType::get(Void, mi_param_types_b, false);
  FunctionType *mi_fty_c = FunctionType::get(Void, mi_param_types_c, false);
  FunctionType *mi_fty_d = FunctionType::get(Void, mi_param_types_d, false);

  m.getOrInsertFunction("SLAMP_llvm_memcpy_p0i8_p0i8_i32", mi_fty_a);
  m.getOrInsertFunction("SLAMP_llvm_memcpy_p0i8_p0i8_i64", mi_fty_b);

  m.getOrInsertFunction("SLAMP_llvm_memmove_p0i8_p0i8_i32", mi_fty_a);
  m.getOrInsertFunction("SLAMP_llvm_memmove_p0i8_p0i8_i64", mi_fty_b);

  m.getOrInsertFunction("SLAMP_llvm_memset_p0i8_i32", mi_fty_c);
  m.getOrInsertFunction("SLAMP_llvm_memset_p0i8_i64", mi_fty_d);

  if (callee_name == "llvm.memcpy.p0i8.p0i8.i32" ||
      callee_name == "llvm.memcpy.p0i8.p0i8.i64" ||
      callee_name == "llvm.memmove.p0i8.p0i8.i32" ||
      callee_name == "llvm.memmove.p0i8.p0i8.i64" ||
      callee_name == "llvm.memset.p0i8.i32" ||
      callee_name == "llvm.memset.p0i8.i64") {
    // good
  } else {
    assert(false && "Unknown memory intrinsic");
  }

  // get corresponding SLAMP runtime function by manipulating callee_name
  ostringstream name;
  name << "SLAMP_";
  for (char i : callee_name) {
    if (i == '.')
      name << '_';
    else
      name << i;
  }
  Function *fcn = m.getFunction(name.str());

  // set parameters

  vector<Value *> args;

  if (callee_name.find("memset") != string::npos) {
    // memset
    args.push_back(cb.getArgOperand(0));
    args.push_back(cb.getArgOperand(2));
  } else {
    // memcpy and memmove
    args.push_back(cb.getArgOperand(0));
    args.push_back(cb.getArgOperand(1));
    args.push_back(cb.getArgOperand(2));
  }

  updateDebugInfo(CallInst::Create(fcn, args, "", mi), mi, m);
}

/// handle `llvm.lifetime.start/end.p0i8`
void SLAMP::instrumentLifetimeIntrinsics(Module &m, Instruction *inst) {
  CallBase &cb = cast<CallBase>(*inst);
  const Function *callee = cb.getCalledFunction();
  assert(callee);
  string callee_name = callee->getName().str();

  // add intrinsic handlers
  Type *mi_param_types_a[] = {I64, I8Ptr};

  FunctionType *mi_fty_a = FunctionType::get(Void, mi_param_types_a, false);

  m.getOrInsertFunction("SLAMP_llvm_lifetime_start_p0i8", mi_fty_a);
  m.getOrInsertFunction("SLAMP_llvm_lifetime_end_p0i8", mi_fty_a);

  if (callee_name == "llvm.lifetime.start.p0i8" ||
      callee_name == "llvm.lifetime.end.p0i8") {
    // good
  } else {
    assert(false && "Unknown lifetime intrinsic");
  }

  // get corresponding SLAMP runtime function by manipulating callee_name
  // replace "." with "_"
  ostringstream name;
  name << "SLAMP_";
  for (char i : callee_name) {
    if (i == '.')
      name << '_';
    else
      name << i;
  }
  Function *fcn = m.getFunction(name.str());

  // set parameters
  vector<Value *> args;
  args.push_back(cb.getArgOperand(0));
  args.push_back(cb.getArgOperand(1));

  CallInst::Create(fcn, args, "", inst);
}

/// handle each instruction (load, store, callbase) in the targeted loop
void SLAMP::instrumentLoopInst(Module &m, Instruction *inst, uint32_t id) {
  if (IgnoreCall) {
    if (isa<CallBase>(inst)) {
      LLVM_DEBUG(errs() << "SLAMP: ignore call " << *inst << "\n");
      return;
    }
  }
  // if elided
  if (elidedLoopInsts.count(inst)) {
    LLVM_DEBUG(errs() << "SLAMP: elided " << *inst << "\n");
    return;
  } else {
    LLVM_DEBUG(if (inst->mayReadOrWriteMemory()) {
      errs() << "SLAMP: instrument " << *inst << "\n";
    });
  }

  const DataLayout &DL = m.getDataLayout();

  // assert(id < INST_ID_BOUND);

  if (id == 0) // instrumented instructions
    return;

  // FIXME: need to handle 16 bytes naturally
  // --- loads
  string lf_name[] = {"SLAMP_load1", "SLAMP_load2", "SLAMP_load4",
                      "SLAMP_load8", "SLAMP_loadn"};
  vector<Function *> lf(5);

  for (unsigned i = 0; i < 5; i++) {
    // load1-8 the last argument is the value, use to do prediction
    // loadn the last argument is the length
    lf[i] = cast<Function>(
        m.getOrInsertFunction(lf_name[i], Void, I32, I64, I32, I64)
            .getCallee());
  }

  // --- stores
  string sf_name[] = {"SLAMP_store1", "SLAMP_store2", "SLAMP_store4",
                      "SLAMP_store8", "SLAMP_storen"};
  vector<Function *> sf(5);

  for (unsigned i = 0; i < 4; i++) {
    sf[i] = cast<Function>(
        m.getOrInsertFunction(sf_name[i], Void, I32, I64).getCallee());
  }
  sf[4] = cast<Function>(
      m.getOrInsertFunction(sf_name[4], Void, I32, I64, I64).getCallee());

  // --- calls

  auto *push = cast<Function>(
      m.getOrInsertFunction("SLAMP_push", Void, I32).getCallee());
  auto *pop =
      cast<Function>(m.getOrInsertFunction("SLAMP_pop", Void).getCallee());

  if (auto *li = dyn_cast<LoadInst>(inst)) {
    // if the loaded pointer is a global
    if (isa<GlobalVariable>(li->getPointerOperand())) {
      if (!ProfileGlobals) {
        LLVM_DEBUG(errs() << "SLAMP: ignore global load " << *li << "\n");
        return;
      }
    }

    InstInsertPt pt = InstInsertPt::After(li);

    Value *ptr = li->getPointerOperand();
    vector<Value *> args;

    args.push_back(ConstantInt::get(I32, id));
    args.push_back(castToInt64Ty(ptr, pt));

    size_t size;
    int index = getIndex(cast<PointerType>(ptr->getType()), size, DL);

    if (index == 4) {
      args.push_back(ConstantInt::get(I32, id));
      args.push_back(ConstantInt::get(I64, size)); // size
    } else {
      args.push_back(ConstantInt::get(I32, id));
      args.push_back(castToInt64Ty(li, pt)); // value
    }

    pt << updateDebugInfo(CallInst::Create(lf[index], args), li, m);
  } else if (auto *si = dyn_cast<StoreInst>(inst)) {
    // if the stored pointer is a global
    if (isa<GlobalVariable>(si->getPointerOperand())) {
      if (!ProfileGlobals) {
        LLVM_DEBUG(errs() << "SLAMP: ignore global store " << *si << "\n");
        return;
      }
    }

    InstInsertPt pt = InstInsertPt::After(si);

    Value *ptr = si->getPointerOperand();
    vector<Value *> args;

    args.push_back(ConstantInt::get(I32, id));
    args.push_back(castToInt64Ty(ptr, pt));

    size_t size;
    int index = getIndex(cast<PointerType>(ptr->getType()), size, DL);

    if (index == 4) {
      args.push_back(ConstantInt::get(I64, size));
    }
    pt << updateDebugInfo(CallInst::Create(sf[index], args), si, m);
  } else if (auto *ci = dyn_cast<CallBase>(inst)) {
    // TODO: this whole "SLAMP_push" and pop are not used now

    // if is LLVM intrinsics
    auto func = ci->getCalledFunction();
    // if indirect call, need to protect
    if (func != nullptr) {
      if (func->isIntrinsic()) {
        LLVM_DEBUG(errs() << "SLAMP: ignore intrinsic " << *ci << "\n");
        return;
      }
      // if is declaration, cannot do anything
      if (func->isDeclaration()) {
        LLVM_DEBUG(errs() << "SLAMP: ignore declaration " << *ci << "\n");
        return;
      }
    }

    // need to handle call and invoke
    vector<Value *> args;

    args.push_back(ConstantInt::get(I32, id));

    InstInsertPt pt = InstInsertPt::Before(ci);
    pt << updateDebugInfo(CallInst::Create(push, args), pt.getPosition(), m);

    if (isa<CallInst>(inst)) {
      pt = InstInsertPt::After(ci);
      pt << updateDebugInfo(CallInst::Create(pop), pt.getPosition(), m);
    } else if (auto *invokeI = dyn_cast<InvokeInst>(inst)) {
      // for invoke, need to find the two paths and add pop
      auto insertPop = [&pop, &m](BasicBlock *entry) {
        InstInsertPt pt;
        if (isa<LandingPadInst>(entry->getFirstNonPHI()))
          pt = InstInsertPt::After(entry->getFirstNonPHI());
        else
          pt = InstInsertPt::Before(entry->getFirstNonPHI());

        pt << updateDebugInfo(CallInst::Create(pop), pt.getPosition(), m);
      };

      insertPop(invokeI->getNormalDest());
      // FIXME: will generate mulitiple `slamp_pop` after the landing pad
      //        Fine for now because `slamp_pop` only set the context to 0
      insertPop(invokeI->getUnwindDest());

    } else {
      assert(false && "Call but not CallInst nor InvokeInst");
    }
  }
}

/// Handle each instruction (load, store) outside of the targeted loop.
/// We don't care about about call insts in this case
void SLAMP::instrumentExtInst(Module &m, Instruction *inst, uint32_t id) {
  // --- loads
  const DataLayout &DL = m.getDataLayout();

  string lf_name[] = {"SLAMP_load1_ext", "SLAMP_load2_ext", "SLAMP_load4_ext",
                      "SLAMP_load8_ext", "SLAMP_loadn_ext"};
  vector<Function *> lf(5);

  for (unsigned i = 0; i < 5; i++) {
    // lf[i] = cast<Function>( m.getOrInsertFunction(lf_name[i], Void, I64, I32,
    // I64, (Type*)0) );
    lf[i] = cast<Function>(
        m.getOrInsertFunction(lf_name[i], Void, I64, I32, I64).getCallee());
  }

  // --- stores
  string sf_name[] = {"SLAMP_store1_ext", "SLAMP_store2_ext",
                      "SLAMP_store4_ext", "SLAMP_store8_ext",
                      "SLAMP_storen_ext"};
  vector<Function *> sf(5);

  for (unsigned i = 0; i < 4; i++) {
    // sf[i] = cast<Function>( m.getOrInsertFunction(sf_name[i], Void, I64, I32,
    // (Type*)0) );
    sf[i] = cast<Function>(
        m.getOrInsertFunction(sf_name[i], Void, I64, I32).getCallee());
  }
  // sf[4] = cast<Function>( m.getOrInsertFunction(sf_name[4], Void, I64, I32,
  // I64, (Type*)0) );
  sf[4] = cast<Function>(
      m.getOrInsertFunction(sf_name[4], Void, I64, I32, I64).getCallee());

  if (auto *li = dyn_cast<LoadInst>(inst)) {
    // if the loaded pointer is a global
    if (isa<GlobalVariable>(li->getPointerOperand())) {
      if (!ProfileGlobals) {
        LLVM_DEBUG(errs() << "SLAMP: ignore global load " << *li << "\n");
        return;
      }
    }

    InstInsertPt pt = InstInsertPt::After(li);

    Value *ptr = li->getPointerOperand();
    vector<Value *> args;

    args.push_back(castToInt64Ty(ptr, pt));

    size_t size;
    int index = getIndex(cast<PointerType>(ptr->getType()), size, DL);

    if (index == 4) {
      args.push_back(ConstantInt::get(I32, id));
      args.push_back(ConstantInt::get(I64, size));
    } else {
      args.push_back(ConstantInt::get(I32, id));
      args.push_back(castToInt64Ty(li, pt));
    }

    pt << updateDebugInfo(CallInst::Create(lf[index], args), li, m);

  } else if (auto *si = dyn_cast<StoreInst>(inst)) {
    // if the stored pointer is a global
    if (isa<GlobalVariable>(si->getPointerOperand())) {
      if (!ProfileGlobals) {
        LLVM_DEBUG(errs() << "SLAMP: ignore global store " << *si << "\n");
        return;
      }
    }
    InstInsertPt pt = InstInsertPt::After(si);

    Value *ptr = si->getPointerOperand();
    vector<Value *> args;

    args.push_back(castToInt64Ty(ptr, pt));

    size_t size;
    int index = getIndex(cast<PointerType>(ptr->getType()), size, DL);

    args.push_back(ConstantInt::get(I32, id));

    if (index == 4) {
      args.push_back(ConstantInt::get(I64, size));
    }

    pt << updateDebugInfo(CallInst::Create(sf[index], args), si, m);
  }
}

/// FIXME: hand-roll an implementation for SLAMP___error_location because
/// __errno_location won't compile
void SLAMP::addWrapperImplementations(Module &m) {
  LLVMContext &c = m.getContext();
  vector<Value *> args;

  // --- SLAMP___errno_location
  auto *f0 = cast<Function>(
      m.getOrInsertFunction("SLAMP___errno_location", I32->getPointerTo())
          .getCallee());
  BasicBlock *entry = BasicBlock::Create(c, "entry", f0, nullptr);
  auto *c0 = cast<Function>(
      m.getOrInsertFunction("__errno_location", I32->getPointerTo())
          .getCallee());
  InstInsertPt pt = InstInsertPt::Beginning(entry);
  auto ci = updateDebugInfo(CallInst::Create(c0, ""), pt.getPosition(), m);
  pt << ci;
  pt << updateDebugInfo(ReturnInst::Create(c, ci), pt.getPosition(), m);
}

} // namespace liberty::slamp
