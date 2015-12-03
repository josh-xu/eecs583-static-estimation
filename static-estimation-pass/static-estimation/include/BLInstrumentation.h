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

using namespace llvm;

// ---------------------------------------------------------------------------
// BLInstrumentationNode extends BallLarusNode with member used by the
// instrumentation algortihms.
// ---------------------------------------------------------------------------
class BLInstrumentationNode : public BallLarusNode {
public:
  // Creates a new BLInstrumentationNode from a BasicBlock.
  BLInstrumentationNode(BasicBlock* BB);

  // Get/sets the Value corresponding to the pathNumber register,
  // constant or phinode.  Used by the instrumentation code to remember
  // path number Values.
  Value* getStartingPathNumber();
  void setStartingPathNumber(Value* pathNumber);

  Value* getEndingPathNumber();
  void setEndingPathNumber(Value* pathNumber);

  // Get/set the PHINode Instruction for this node.
  PHINode* getPathPHI();
  void setPathPHI(PHINode* pathPHI);

private:

  Value* _startingPathNumber; // The Value for the current pathNumber.
  Value* _endingPathNumber; // The Value for the current pathNumber.
  PHINode* _pathPHI; // The PHINode for current pathNumber.
};

// --------------------------------------------------------------------------
// BLInstrumentationEdge extends BallLarusEdge with data about the
// instrumentation that will end up on each edge.
// --------------------------------------------------------------------------
class BLInstrumentationEdge : public BallLarusEdge {
public:
  BLInstrumentationEdge(BLInstrumentationNode* source,
                        BLInstrumentationNode* target);

  // Sets the target node of this edge.  Required to split edges.
  void setTarget(BallLarusNode* node);

  // Get/set whether edge is in the spanning tree.
  bool isInSpanningTree() const;
  void setIsInSpanningTree(bool isInSpanningTree);

  // Get/ set whether this edge will be instrumented with a path number
  // initialization.
  bool isInitialization() const;
  void setIsInitialization(bool isInitialization);

  // Get/set whether this edge will be instrumented with a path counter
  // increment.  Notice this is incrementing the path counter
  // corresponding to the path number register.  The path number
  // increment is determined by getIncrement().
  bool isCounterIncrement() const;
  void setIsCounterIncrement(bool isCounterIncrement);

  // Get/set the path number increment that this edge will be instrumented
  // with.  This is distinct from the path counter increment and the
  // weight.  The counter increment counts the number of executions of
  // some path, whereas the path number keeps track of which path number
  // the program is on.
  long getIncrement() const;
  void setIncrement(long increment);

  // Get/set whether the edge has been instrumented.
  bool hasInstrumentation();
  void setHasInstrumentation(bool hasInstrumentation);

  // Returns the successor number of this edge in the source.
  unsigned getSuccessorNumber();

private:
  // The increment that the code will be instrumented with.
  long long _increment;

  // Whether this edge is in the spanning tree.
  bool _isInSpanningTree;

  // Whether this edge is an initialiation of the path number.
  bool _isInitialization;

  // Whether this edge is a path counter increment.
  bool _isCounterIncrement;

  // Whether this edge has been instrumented.
  bool _hasInstrumentation;
};

// ---------------------------------------------------------------------------
// BLInstrumentationDag extends BallLarusDag with algorithms that
// determine where instrumentation should be placed.
// ---------------------------------------------------------------------------
class BLInstrumentationDag : public BallLarusDag {
public:
  BLInstrumentationDag(Function &F);

  // Returns the Exit->Root edge. This edge is required for creating
  // directed cycles in the algorithm for moving instrumentation off of
  // the spanning tree
  BallLarusEdge* getExitRootEdge();

  // Returns an array of phony edges which mark those nodes
  // with function calls
  BLEdgeVector getCallPhonyEdges();

  // Gets/sets the path counter array
  GlobalVariable* getCounterArray();
  void setCounterArray(GlobalVariable* c);

  // Calculates the increments for the chords, thereby removing
  // instrumentation from the spanning tree edges. Implementation is based
  // on the algorithm in Figure 4 of [Ball94]
  void calculateChordIncrements();

  // Updates the state when an edge has been split
  void splitUpdate(BLInstrumentationEdge* formerEdge, BasicBlock* newBlock);

  // Calculates a spanning tree of the DAG ignoring cycles.  Whichever
  // edges are in the spanning tree will not be instrumented, but this
  // implementation does not try to minimize the instrumentation overhead
  // by trying to find hot edges.
  void calculateSpanningTree();

  // Pushes initialization further down in order to group the first
  // increment and initialization.
  void pushInitialization();

  // Pushes the path counter increments up in order to group the last path
  // number increment.
  void pushCounters();

  // Removes phony edges from the successor list of the source, and the
  // predecessor list of the target.
  void unlinkPhony();

  // Generate dot graph for the function
  void generateDotGraph();

protected:
  // BLInstrumentationDag creates BLInstrumentationNode objects in this
  // method overriding the creation of BallLarusNode objects.
  //
  // Allows subclasses to determine which type of Node is created.
  // Override this method to produce subclasses of BallLarusNode if
  // necessary.
  virtual BallLarusNode* createNode(BasicBlock* BB);

  // BLInstrumentationDag create BLInstrumentationEdges.
  //
  // Allows subclasses to determine which type of Edge is created.
  // Override this method to produce subclasses of BallLarusEdge if
  // necessary.  Parameters source and target will have been created by
  // createNode and can be cast to the subclass of BallLarusNode*
  // returned by createNode.
  virtual BallLarusEdge* createEdge(
    BallLarusNode* source, BallLarusNode* target, unsigned edgeNumber);

private:
  BLEdgeVector _treeEdges; // All edges in the spanning tree.
  BLEdgeVector _chordEdges; // All edges not in the spanning tree.
  GlobalVariable* _counterArray; // Array to store path counters

  // Removes the edge from the appropriate predecessor and successor lists.
  void unlinkEdge(BallLarusEdge* edge);

  // Makes an edge part of the spanning tree.
  void makeEdgeSpanning(BLInstrumentationEdge* edge);

  // Pushes initialization and calls itself recursively.
  void pushInitializationFromEdge(BLInstrumentationEdge* edge);

  // Pushes path counter increments up recursively.
  void pushCountersFromEdge(BLInstrumentationEdge* edge);

  // Depth first algorithm for determining the chord increments.f
  void calculateChordIncrementsDfs(
    long weight, BallLarusNode* v, BallLarusEdge* e);

  // Determines the relative direction of two edges.
  int calculateChordIncrementsDir(BallLarusEdge* e, BallLarusEdge* f);
};

