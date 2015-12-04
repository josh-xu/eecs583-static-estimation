// This pass extracts LSTM features in a similar method to the static estimator pass. 
// Instead of creating a feature vector for each path, however, we create a (simple)
// feature vector for each basic block and will use those BB's as the sequences to the LSTM
#include "llvm/Transforms/Instrumentation.h"
#include "ProfilingUtils.h"
#include "llvm/Analysis/PathNumbering.h"
#include "llvm/Analysis/PathProfileInfo.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/TypeBuilder.h"
#include "llvm/Pass.h"
#include "llvm/PassManager.h"
#include "llvm/Support/CFG.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

#include <fstream>
#include <vector>

#include "BLInstrumentation.h"
#include "FeatureExtractor.h"

using namespace llvm;

class LSTMStaticEstimatorPass : public ModulePass {
private:
  // Profiling
  PathProfileInfo* PI;

  // File for output
  std::ofstream ofs;

  // Instruments each function with path profiling.  'main' is instrumented
  // with code to save the profile to disk.
  bool runOnModule(Module &M);

  // Calculates all paths for a dag
  void calculatePaths(Function* fn);

  // Analyzes the function for Ball-Larus path profiling, and inserts code.
  void runOnFunction(std::vector<Constant*> &ftInit, Function &F, Module &M);

  // To use profiling info
  void getAnalysisUsage(AnalysisUsage &AU) const;


public:
  static char ID; // Pass identification, replacement for typeid
  LSTMStaticEstimatorPass() : ModulePass(ID) {
    //initializeLSTMStaticEstimatorPass(*PassRegistry::getPassRegistry());
  }

  virtual const char *getPassName() const {
    return "Static Estimation";
  }
};

// Iterate through all possible paths in the dag
void LSTMStaticEstimatorPass::calculatePaths(Function* fn) {
  std::vector<BasicBlock*> path;

  PI->setCurrentFunction(fn);
  unsigned nPaths = PI->getPotentialPathCount();
  unsigned nPathsRun = PI->pathsRun();
  errs() << "There are " << nPaths << " paths\n";
  if (nPathsRun == 0) {
      errs() << "This function is never run in profiling! Skipping...\n";
  }
  else {
      // Enumerate all paths in this function
      for (int i=0; i<nPaths; i++) {
          // Show progress for large values
          if (i % 10000 == 0)
              errs() << "Computed for " << i << "/" << nPaths << " paths\n";

          ProfilePath* curPath = PI->getPath(i);
          unsigned n_real_count = 0;
          if (curPath) {
              path = *(curPath->getPathBlocks());
              n_real_count = curPath->getCount();
          }

          // Extract features 
          FeatureExtractor* features = new FeatureExtractor(path);
          std::string fnName = fn->getName();
          ofs << fnName << "_" << i << " "                  // Function ID
              << n_real_count << " "                        // Ground truth
              << path.size() << "\n"                        // Number of BB to follow
              << features->getFeaturesLSTM();               // BBs and features
          delete features;
      }
  }
}

// Entry point of the module
void LSTMStaticEstimatorPass::runOnFunction(std::vector<Constant*> &ftInit,
                                 Function &F, Module &M) {
  errs() << "Running on function " << F.getName() << "\n";

  // Calculate the features for each path 
  calculatePaths(&F);
}

bool LSTMStaticEstimatorPass::runOnModule(Module &M) {
  errs() << "Running research module\n";

  PI = &getAnalysis<PathProfileInfo>();

  // Start outputs
  std::string fname = "feature_output.csv";
  errs() << "Writing to " << fname << "\n";
  ofs.open(fname, std::ofstream::out);

  // No main, no instrumentation!
  Function *Main = M.getFunction("main");
  if (!Main)
    Main = M.getFunction("MAIN__");

  if (!Main) {
    errs() << "WARNING: cannot run static estimation on a module"
           << " with no main function!\n";
    return false;
  }

  std::vector<Constant*> ftInit;
  unsigned functionNumber = 0;
  for (Module::iterator F = M.begin(), E = M.end(); F != E; F++) {
    if (F->isDeclaration())
      continue;

    runOnFunction(ftInit, *F, M);
  }

  ofs.close();
  return false;
}

void LSTMStaticEstimatorPass::getAnalysisUsage(AnalysisUsage &AU) const {
    AU.addRequired<PathProfileInfo>();
}

// Register the path profiler as a pass
char LSTMStaticEstimatorPass::ID = 0;
static RegisterPass<LSTMStaticEstimatorPass> X("LSTMStaticEstimatorPass", "insert-lstm-static-estimation", false, false);
