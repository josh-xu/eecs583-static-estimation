#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

#include <set>

#include "FeatureExtractor.h"

using namespace llvm;

namespace {
  struct StaticEstimationPass : public FunctionPass {
    static char ID;
    StaticEstimationPass() : FunctionPass(ID) {}

    virtual bool runOnFunction(Function &F) {
      std::set<Instruction*> IRList;
      for (auto I = inst_begin(F), E = inst_end(F); I != E; ++I) {
        IRList.insert(&*I);
      }
      FeatureExtractor* features = new FeatureExtractor(IRList);
      features->extractFeatures();
      errs() << "I saw a function called " << F.getName() << "!\n";
      for (auto inst : features->getFeatures()) {
        errs() << inst.first << " -> " << (int)(inst.second) << "\n";
      }
      return false;
    }
  };
}

char StaticEstimationPass::ID = 0;

// Automatically enable the pass.
// http://adriansampson.net/blog/clangpass.html
static void registerStaticEstimationPass(const PassManagerBuilder &,
                         legacy::PassManagerBase &PM) {
  PM.add(new StaticEstimationPass());
}
static RegisterStandardPasses
  RegisterMyPass(PassManagerBuilder::EP_EarlyAsPossible,
                 registerStaticEstimationPass);
