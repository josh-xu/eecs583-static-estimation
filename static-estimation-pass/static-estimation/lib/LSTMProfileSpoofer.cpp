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
#include <unordered_map>

#include "llvm/Analysis/Passes.h"
#include "llvm/ADT/SmallSet.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Analysis/ProfileInfo.h"
#include "llvm/Analysis/ProfileInfoLoader.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/Support/Format.h"
#include <set>

#include "BLInstrumentation.h"

#define MAX_PATHS 1000

using namespace llvm;

namespace {
  class LSTMProfileSpooferPass : public ModulePass, public ProfileInfo {
  private:
    // Instruments each function with path profiling.  'main' is instrumented
    // with code to save the profile to disk.
    bool runOnModule(Module &M);

    // Calculate the path for a single ID
    std::vector<BasicBlock*> computePath(BLInstrumentationDag* dag, unsigned pathNo);

    // Calculates all paths for a dag
    void calculatePaths(BLInstrumentationDag* dag);

    // Analyzes the function for Ball-Larus path profiling, and inserts code.
    void runOnFunction(std::vector<Constant*> &ftInit, Function &F, Module &M);

    // Path ID to Hotness
    std::map<std::string, std::map<int, double>> hotness;

    // File for input of static profile
    std::ifstream ifs;

  public:
    static char ID; // Pass identification, replacement for typeid
    explicit LSTMProfileSpooferPass(const std::string &filename = "") : ModulePass(ID) {
      //initializeLSTMProfileSpooferPassPass(*PassRegistry::getPassRegistry());
    }

    virtual const char *getPassName() const {
      return "LSTM Profile Spoofer";
    }

    virtual void getAnalysisUsage(AnalysisUsage &AU) const {
        AU.setPreservesAll();
      }

    virtual void *getAdjustedAnalysisPointer(AnalysisID PI) {
      if (PI == &ProfileInfo::ID)
        return (ProfileInfo*)this;
      return this;
    }
  };
} // End of anonymous namespace

// Register the path profiler as an analysis group pass
char LSTMProfileSpooferPass::ID = 0;
// INITIALIZE_AG_PASS(LSTMProfileSpooferPass, ProfileInfo, "profile-spoofer",
//               "Spoof profile information as ProfileInfo", false, true, false);

static RegisterPass<LSTMProfileSpooferPass> X("profile-spoofer", "Spoof profile information as ProfileInfo", false, true);
static RegisterAnalysisGroup<ProfileInfo> Y(X);

char &llvm::ProfileLoaderPassID = LSTMProfileSpooferPass::ID;
ModulePass *llvm::createProfileLoaderPass() { return new LSTMProfileSpooferPass(); }

/// createProfileLoaderPass - This function returns a Pass that loads the
/// profiling information for the module from the specified filename, making it
/// available to the optimizers.
Pass *llvm::createProfileLoaderPass(const std::string &Filename) {
  return new LSTMProfileSpooferPass(Filename);
}

// Compute the path through the DAG from its path number
std::vector<BasicBlock*> LSTMProfileSpooferPass::computePath(BLInstrumentationDag* dag, unsigned pathNo) {
    unsigned R = pathNo;
    std::vector<BasicBlock*> path;

    BLInstrumentationNode* prev2;
    BLInstrumentationNode* prev1;
    //std::unordered_map<BLInstrumentationNode*,int> visited;

    BLInstrumentationNode* curNode = (BLInstrumentationNode*)(dag->getRoot());
    while (1) {
        BLInstrumentationEdge* nextEdge;
        unsigned bestEdge = 0;
        // Add the basic block to the list

        path.push_back(curNode->getBlock());
  //visited[curNode] = 1;
  //errs() << "next edge options: ";
        
  for (BLEdgeIterator next = curNode->succBegin(), end = curNode->succEnd(); next != end; next++) {
            // We want the largest edge that's less than R
            BLInstrumentationEdge* i = (BLInstrumentationEdge*) *next;
      //errs() << i << " ";
            unsigned weight = i->getWeight();
            //if (weight <= R && weight >= bestEdge && visited.find((BLInstrumentationNode*)i->getTarget()) == visited.end()) 
      if (weight <= R && weight >= bestEdge && prev1 != (BLInstrumentationNode*)i->getTarget()) {
                bestEdge = weight;
                nextEdge = i;
            }
      
      if (R == 0 && (BLInstrumentationNode*)i->getTarget() == (BLInstrumentationNode*)dag->getExit()) {
    //errs() << "Found exit node with R=0!\n";
    bestEdge = weight;
    nextEdge = i;
    break;
      }
      
        }
  //errs() << "\n";
  
        BLInstrumentationNode* nextNode = (BLInstrumentationNode*)(nextEdge->getTarget());       

  //errs() << "R = " << R << " nextNode: " << nextNode << " bestEdge = " << bestEdge << "\n";   

  if (nextNode == (BLInstrumentationNode*)dag->getExit()) 
    break;

  // Terminate on the <null> 
  //if (!nextNode->getBlock())
        //  break;
        // Move to next node
  //prevent loops when weight = 0
  prev2 = prev1;
  prev1 = curNode;

        curNode = nextNode;
        R -= bestEdge;
    }
    return path;
}
// Iterate through all possible paths in the dag
void LSTMProfileSpooferPass::calculatePaths(BLInstrumentationDag* dag) {
  unsigned nPaths = dag->getNumberOfPaths();
  errs() << "There are " << nPaths << " paths\n";

  int stride = nPaths / MAX_PATHS;
  if (stride <= 1)
      stride = 1;

  errs() << "Using stride " << stride << "\n";

  Function* fn = dag->getRoot()->getBlock()->getParent();

  int n_extracted = 0;
  // Enumerate all paths in this function
  for (int i=0; i<nPaths; i++) {
      // Show progress for large values
      if (i % 100000 == 0 && i != 0) {
          errs() << "Computed for " << i << "/" << nPaths << " paths\n";
      }

      std::vector<BasicBlock*> path = computePath(dag, i);
      // ProfilePath* curPath = PI->getPath(i);
      // unsigned n_real_count = 0;
      // if (curPath) {
      //     n_real_count = curPath->getCount();
      // }

      if (hotness.find(fn->getName()) != hotness.end()) {
          if(hotness[fn->getName()].find(i) != hotness[fn->getName()].end()){
            for(int j = 0; j < path.size(); j++){
              BlockInformation[fn][path[j]] += hotness[fn->getName()][i];
            }
          }
      }
  }
  errs() << "Extracted " << n_extracted << " paths for this function\n\n";
}

// Entry point of the module
void LSTMProfileSpooferPass::runOnFunction(std::vector<Constant*> &ftInit,
                                 Function &F, Module &M) {
  errs() << "Running on function " << F.getName() << "\n";

  /*
  if (F.getName().compare("BZ2_decompress")) {
    errs() << "Skipping...\n";
    return;
  }
  */    


  // Build DAG from CFG
  BLInstrumentationDag dag = BLInstrumentationDag(F);
  dag.init();

  // give each path a unique integer value
  dag.calculatePathNumbers();

  errs() << "Starting calculatePaths..." << "\n";
 
  // Calculate the features for each path 
  calculatePaths(&dag);
}

bool LSTMProfileSpooferPass::runOnModule(Module &M) {
  errs() << "Running research module\n";

  EdgeInformation.clear();
  BlockInformation.clear();
  FunctionInformation.clear();

  errs() << "about to open file\n";
  ifs.open("static_predictions.csv");
  errs() << "tried to open file\n";
  std::string fName;
  int pathID;
  double count;
  std::string line;
  std::getline(ifs,line);
  while(std::getline(ifs,line)){
    fName = line.substr(0, line.find(" "));
    line = line.substr(line.find(" ")+1);
    pathID = std::stoi(line.substr(0, line.find(",")));
    count = std::stod(line.substr(line.find(",")+1));
    hotness[fName][pathID] = count;
  }
  ifs.close();

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

  for(std::map<std::string, std::map<int, double>>::iterator i = hotness.begin(); i != hotness.end(); ++i){
    for(std::map<int, double>::iterator j = i->second.begin(); j != i->second.end(); ++j){
      errs() << i->first << ", " << j->first << ", " << j->second << "\n";
    }
  }

  for(std::map<const Function*, std::map<const BasicBlock*, double>>::iterator i = BlockInformation.begin(); i != BlockInformation.end(); ++i){
    for(std::map<const BasicBlock*, double>::iterator j = i->second.begin(); j != i->second.end(); ++j){
      errs() << i->first->getName() << ", " << j->first->getName() << ", " << j->second << "\n";
    }
  }
  return false;
}

