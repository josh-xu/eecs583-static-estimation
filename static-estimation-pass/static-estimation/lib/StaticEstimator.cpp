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

class StaticEstimatorPass : public ModulePass {
private:
  // Profiling
  PathProfileInfo* PI;

  // File for output
  std::ofstream ofs;

  // Instruments each function with path profiling.  'main' is instrumented
  // with code to save the profile to disk.
  bool runOnModule(Module &M);

  // Calculate the path for a single ID
  std::vector<BasicBlock*> computePath(BLInstrumentationDag* dag, unsigned pathNo);

  // Calculates all paths for a dag
  void calculatePaths(BLInstrumentationDag* dag);

  // Analyzes the function for Ball-Larus path profiling, and inserts code.
  void runOnFunction(std::vector<Constant*> &ftInit, Function &F, Module &M);

  // To use profiling info
  void getAnalysisUsage(AnalysisUsage &AU) const;


public:
  static char ID; // Pass identification, replacement for typeid
  StaticEstimatorPass() : ModulePass(ID) {
    //initializeStaticEstimatorPass(*PassRegistry::getPassRegistry());
  }

  virtual const char *getPassName() const {
    return "Static Estimation";
  }
};

// Compute the path through the DAG from its path number
std::vector<BasicBlock*> StaticEstimatorPass::computePath(BLInstrumentationDag* dag, unsigned pathNo) {
    unsigned R = pathNo;
    std::vector<BasicBlock*> path;

    BLInstrumentationNode* curNode = (BLInstrumentationNode*)(dag->getRoot());
    while (1) {
        BLInstrumentationEdge* nextEdge;
        unsigned bestEdge = 0;
        // Add the basic block to the list
        path.push_back(curNode->getBlock());
        for (BLEdgeIterator next = curNode->succBegin(), end = curNode->succEnd(); next != end; next++) {
            // We want the largest edge that's less than R
            BLInstrumentationEdge* i = (BLInstrumentationEdge*) *next;
            unsigned weight = i->getWeight();
            if (weight <= R && weight >= bestEdge) {
                bestEdge = weight;
                nextEdge = i;
            }
        }
        BLInstrumentationNode* nextNode = (BLInstrumentationNode*)(nextEdge->getTarget());
        // Terminate on the <null> 
        if (!nextNode->getBlock())
            break;
        // Move to next node
        curNode = nextNode;
        R -= bestEdge;
    }
    return path;
}

// Iterate through all possible paths in the dag
void StaticEstimatorPass::calculatePaths(BLInstrumentationDag* dag) {
  unsigned nPaths = dag->getNumberOfPaths();
  errs() << "There are " << nPaths << " paths\n";

  Function* fn = dag->getRoot()->getBlock()->getParent();
  PI->setCurrentFunction(fn);
  unsigned nPathsRun = PI->pathsRun();
  if (nPathsRun == 0) {
      errs() << "This function is never run in profiling! Skipping...\n";
  }
  else {
      // Enumerate all paths in this function
      for (int i=0; i<nPaths; i++) {
          // Show progress for large values
          if (i % 10000 == 0 && i != 0)
              errs() << "Computed for " << i << "/" << nPaths << " paths\n";

          std::vector<BasicBlock*> path = computePath(dag, i);
          ProfilePath* curPath = PI->getPath(i);
          unsigned n_real_count = 0;
          if (curPath) {
              n_real_count = curPath->getCount();
          }

          // Extract features 
          FeatureExtractor* features = new FeatureExtractor(path);
          features->extractFeatures();
          std::string fnName = fn->getName();
          ofs << fnName << "." << i << ", " << n_real_count << "," << features->getFeaturesCSV();
          delete features;
      }
  }
}

// Entry point of the module
void StaticEstimatorPass::runOnFunction(std::vector<Constant*> &ftInit,
                                 Function &F, Module &M) {
  errs() << "Running on function " << F.getName() << "\n";

  // Build DAG from CFG
  BLInstrumentationDag dag = BLInstrumentationDag(F);
  dag.init();

  // give each path a unique integer value
  dag.calculatePathNumbers();
 
  // Calculate the features for each path 
  calculatePaths(&dag);
}

bool StaticEstimatorPass::runOnModule(Module &M) {
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

  // Run some dummy feature extraction to get the col names
  std::vector<BasicBlock*> tempPath;
  tempPath.push_back(&Main->getEntryBlock());
  FeatureExtractor* features = new FeatureExtractor(tempPath);
  features->extractFeatures();
  ofs << "ID,RealCount," << features->getFeaturesCSVNames();


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

void StaticEstimatorPass::getAnalysisUsage(AnalysisUsage &AU) const {
    AU.addRequired<PathProfileInfo>();
}

// Register the path profiler as a pass
char StaticEstimatorPass::ID = 0;
static RegisterPass<StaticEstimatorPass> X("StaticEstimatorPass", "insert-static-estimation", false, false);
