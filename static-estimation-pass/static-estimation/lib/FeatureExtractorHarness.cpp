#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Format.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

#include <set>

#include "FeatureExtractor.h"

using namespace llvm;

namespace {
  struct FeatureExtractorHarnessPass : public FunctionPass {
    static char ID;
    FeatureExtractorHarnessPass() : FunctionPass(ID) {}

    virtual bool runOnFunction(Function &F) {
      std::set<Instruction*> IRList;
      for (auto b = F.begin(); b != F.end(); b++) {
        for (auto I = b->begin(); I != b->end(); I++) {
          IRList.insert(&*I);
        }
      }
      FeatureExtractor* features = new FeatureExtractor(IRList);
      features->extractFeatures();
      errs() << "I saw a function called " << F.getName() << "!\n";
      for (auto inst : features->getFeatures()) {
        errs() << inst.first << " -> " << format("%.3f", inst.second) << "\n";
      }
      return false;
    }
  };
}

char FeatureExtractorHarnessPass::ID = 0;
static RegisterPass<FeatureExtractorHarnessPass> X("FeatureExtractorHarnessPass", "test-feature-extractor", false, false);
