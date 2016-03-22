#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Format.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

#include <vector>

#include "FeatureExtractor.h"

using namespace llvm;

namespace {
  struct FeatureExtractorHarnessPass : public FunctionPass {
    static char ID;
    FeatureExtractorHarnessPass() : FunctionPass(ID) {}

    virtual bool runOnFunction(Function &F) {
      std::vector<BasicBlock*> BBVector;
      for (auto b = F.begin(); b != F.end(); b++) {
          BBVector.push_back(&*b);
      }
      FeatureExtractor* features = new FeatureExtractor(BBVector);
      features->extractFeatures();
      errs() << "I saw a function called " << F.getName() << "!\n";
      for (auto inst : features->getFeatures()) {
        errs() << inst.first << " -> " << format("%.3f", inst.second) << "\n";
      }
      errs() << features->getFeaturesCSV();
      delete features;
      return false;
    }
  };
}

char FeatureExtractorHarnessPass::ID = 0;
static RegisterPass<FeatureExtractorHarnessPass> X("FeatureExtractorHarnessPass", "test-feature-extractor", false, false);
