#include "BLInstrumentation.h"

namespace llvm {
  class StaticEstimationFunctionTable {};

  // Type for global array storing references to hashes or arrays
  template<bool xcompile> class TypeBuilder<StaticEstimationFunctionTable,
                                            xcompile> {
  public:
    static StructType *get(LLVMContext& C) {
      return( StructType::get(
                TypeBuilder<types::i<32>, xcompile>::get(C), // type
                TypeBuilder<types::i<32>, xcompile>::get(C), // array size
                TypeBuilder<types::i<8>*, xcompile>::get(C), // array/hash ptr
                NULL));
    }
  };

  typedef TypeBuilder<StaticEstimationFunctionTable, true>
  ftEntryTypeBuilder;

  // BallLarusEdge << operator overloading
  raw_ostream& operator<<(raw_ostream& os,
                          const BLInstrumentationEdge& edge)
      LLVM_ATTRIBUTE_USED;
  raw_ostream& operator<<(raw_ostream& os,
                          const BLInstrumentationEdge& edge) {
    os << "[" << edge.getSource()->getName() << " -> "
       << edge.getTarget()->getName() << "] init: "
       << (edge.isInitialization() ? "yes" : "no")
       << " incr:" << edge.getIncrement() << " cinc: "
       << (edge.isCounterIncrement() ? "yes" : "no");
    return(os);
  }
}

// Creates a new BLInstrumentationNode from a BasicBlock.
BLInstrumentationNode::BLInstrumentationNode(BasicBlock* BB) :
  BallLarusNode(BB),
  _startingPathNumber(NULL), _endingPathNumber(NULL), _pathPHI(NULL) {}

// Constructor for BLInstrumentationEdge.
BLInstrumentationEdge::BLInstrumentationEdge(BLInstrumentationNode* source,
                                             BLInstrumentationNode* target)
  : BallLarusEdge(source, target, 0),
    _increment(0), _isInSpanningTree(false), _isInitialization(false),
    _isCounterIncrement(false), _hasInstrumentation(false) {}

// Sets the target node of this edge.  Required to split edges.
void BLInstrumentationEdge::setTarget(BallLarusNode* node) {
  _target = node;
}

// Returns whether this edge is in the spanning tree.
bool BLInstrumentationEdge::isInSpanningTree() const {
  return(_isInSpanningTree);
}

// Sets whether this edge is in the spanning tree.
void BLInstrumentationEdge::setIsInSpanningTree(bool isInSpanningTree) {
  _isInSpanningTree = isInSpanningTree;
}

// Returns whether this edge will be instrumented with a path number
// initialization.
bool BLInstrumentationEdge::isInitialization() const {
  return(_isInitialization);
}

// Sets whether this edge will be instrumented with a path number
// initialization.
void BLInstrumentationEdge::setIsInitialization(bool isInitialization) {
  _isInitialization = isInitialization;
}

// Returns whether this edge will be instrumented with a path counter
// increment.  Notice this is incrementing the path counter
// corresponding to the path number register.  The path number
// increment is determined by getIncrement().
bool BLInstrumentationEdge::isCounterIncrement() const {
  return(_isCounterIncrement);
}

// Sets whether this edge will be instrumented with a path counter
// increment.
void BLInstrumentationEdge::setIsCounterIncrement(bool isCounterIncrement) {
  _isCounterIncrement = isCounterIncrement;
}

// Gets the path number increment that this edge will be instrumented
// with.  This is distinct from the path counter increment and the
// weight.  The counter increment is counts the number of executions of
// some path, whereas the path number keeps track of which path number
// the program is on.
long BLInstrumentationEdge::getIncrement() const {
  return(_increment);
}

// Set whether this edge will be instrumented with a path number
// increment.
void BLInstrumentationEdge::setIncrement(long increment) {
  _increment = increment;
}

// True iff the edge has already been instrumented.
bool BLInstrumentationEdge::hasInstrumentation() {
  return(_hasInstrumentation);
}

// Set whether this edge has been instrumented.
void BLInstrumentationEdge::setHasInstrumentation(bool hasInstrumentation) {
  _hasInstrumentation = hasInstrumentation;
}

// Returns the successor number of this edge in the source.
unsigned BLInstrumentationEdge::getSuccessorNumber() {
  BallLarusNode* sourceNode = getSource();
  BallLarusNode* targetNode = getTarget();
  BasicBlock* source = sourceNode->getBlock();
  BasicBlock* target = targetNode->getBlock();

  if(source == NULL || target == NULL)
    return(0);

  TerminatorInst* terminator = source->getTerminator();

        unsigned i;
  for(i=0; i < terminator->getNumSuccessors(); i++) {
    if(terminator->getSuccessor(i) == target)
      break;
  }

  return(i);
}

// BLInstrumentationDag constructor initializes a DAG for the given Function.
BLInstrumentationDag::BLInstrumentationDag(Function &F) : BallLarusDag(F),
                                                          _counterArray(0) {
}

// Returns the Exit->Root edge. This edge is required for creating
// directed cycles in the algorithm for moving instrumentation off of
// the spanning tree
BallLarusEdge* BLInstrumentationDag::getExitRootEdge() {
  BLEdgeIterator erEdge = getExit()->succBegin();
  return(*erEdge);
}

BLEdgeVector BLInstrumentationDag::getCallPhonyEdges () {
  BLEdgeVector callEdges;

  for( BLEdgeIterator edge = _edges.begin(), end = _edges.end();
       edge != end; edge++ ) {
    if( (*edge)->getType() == BallLarusEdge::CALLEDGE_PHONY )
      callEdges.push_back(*edge);
  }

  return callEdges;
}

// Gets the path counter array
GlobalVariable* BLInstrumentationDag::getCounterArray() {
  return _counterArray;
}

void BLInstrumentationDag::setCounterArray(GlobalVariable* c) {
  _counterArray = c;
}

// Calculates the increment for the chords, thereby removing
// instrumentation from the spanning tree edges. Implementation is based on
// the algorithm in Figure 4 of [Ball94]
void BLInstrumentationDag::calculateChordIncrements() {
  calculateChordIncrementsDfs(0, getRoot(), NULL);

  BLInstrumentationEdge* chord;
  for(BLEdgeIterator chordEdge = _chordEdges.begin(),
      end = _chordEdges.end(); chordEdge != end; chordEdge++) {
    chord = (BLInstrumentationEdge*) *chordEdge;
    chord->setIncrement(chord->getIncrement() + chord->getWeight());
  }
}

// Updates the state when an edge has been split
void BLInstrumentationDag::splitUpdate(BLInstrumentationEdge* formerEdge,
                                       BasicBlock* newBlock) {
  BallLarusNode* oldTarget = formerEdge->getTarget();
  BallLarusNode* newNode = addNode(newBlock);
  formerEdge->setTarget(newNode);
  newNode->addPredEdge(formerEdge);

  DEBUG(dbgs() << "  Edge split: " << *formerEdge << "\n");

  oldTarget->removePredEdge(formerEdge);
  BallLarusEdge* newEdge = addEdge(newNode, oldTarget,0);

  if( formerEdge->getType() == BallLarusEdge::BACKEDGE ||
                        formerEdge->getType() == BallLarusEdge::SPLITEDGE) {
                newEdge->setType(formerEdge->getType());
    newEdge->setPhonyRoot(formerEdge->getPhonyRoot());
    newEdge->setPhonyExit(formerEdge->getPhonyExit());
    formerEdge->setType(BallLarusEdge::NORMAL);
                formerEdge->setPhonyRoot(NULL);
    formerEdge->setPhonyExit(NULL);
  }
}

// Calculates a spanning tree of the DAG ignoring cycles.  Whichever
// edges are in the spanning tree will not be instrumented, but this
// implementation does not try to minimize the instrumentation overhead
// by trying to find hot edges.
void BLInstrumentationDag::calculateSpanningTree() {
  std::stack<BallLarusNode*> dfsStack;

  for(BLNodeIterator nodeIt = _nodes.begin(), end = _nodes.end();
      nodeIt != end; nodeIt++) {
    (*nodeIt)->setColor(BallLarusNode::WHITE);
  }

  dfsStack.push(getRoot());
  while(dfsStack.size() > 0) {
    BallLarusNode* node = dfsStack.top();
    dfsStack.pop();

    if(node->getColor() == BallLarusNode::WHITE)
      continue;

    BallLarusNode* nextNode;
    bool forward = true;
    BLEdgeIterator succEnd = node->succEnd();

    node->setColor(BallLarusNode::WHITE);
    // first iterate over successors then predecessors
    for(BLEdgeIterator edge = node->succBegin(), predEnd = node->predEnd();
        edge != predEnd; edge++) {
      if(edge == succEnd) {
        edge = node->predBegin();
        forward = false;
      }

      // Ignore split edges
      if ((*edge)->getType() == BallLarusEdge::SPLITEDGE)
        continue;

      nextNode = forward? (*edge)->getTarget(): (*edge)->getSource();
      if(nextNode->getColor() != BallLarusNode::WHITE) {
        nextNode->setColor(BallLarusNode::WHITE);
        makeEdgeSpanning((BLInstrumentationEdge*)(*edge));
      }
    }
  }

  for(BLEdgeIterator edge = _edges.begin(), end = _edges.end();
      edge != end; edge++) {
    BLInstrumentationEdge* instEdge = (BLInstrumentationEdge*) (*edge);
      // safe since createEdge is overriden
    if(!instEdge->isInSpanningTree() && (*edge)->getType()
        != BallLarusEdge::SPLITEDGE)
      _chordEdges.push_back(instEdge);
  }
}

// Pushes initialization further down in order to group the first
// increment and initialization.
void BLInstrumentationDag::pushInitialization() {
  BLInstrumentationEdge* exitRootEdge =
                (BLInstrumentationEdge*) getExitRootEdge();
  exitRootEdge->setIsInitialization(true);
  pushInitializationFromEdge(exitRootEdge);
}

// Pushes the path counter increments up in order to group the last path
// number increment.
void BLInstrumentationDag::pushCounters() {
  BLInstrumentationEdge* exitRootEdge =
    (BLInstrumentationEdge*) getExitRootEdge();
  exitRootEdge->setIsCounterIncrement(true);
  pushCountersFromEdge(exitRootEdge);
}

// Removes phony edges from the successor list of the source, and the
// predecessor list of the target.
void BLInstrumentationDag::unlinkPhony() {
  BallLarusEdge* edge;

  for(BLEdgeIterator next = _edges.begin(),
      end = _edges.end(); next != end; next++) {
    edge = (*next);

    if( edge->getType() == BallLarusEdge::BACKEDGE_PHONY ||
        edge->getType() == BallLarusEdge::SPLITEDGE_PHONY ||
        edge->getType() == BallLarusEdge::CALLEDGE_PHONY ) {
      unlinkEdge(edge);
    }
  }
}

// Generate a .dot graph to represent the DAG and pathNumbers
void BLInstrumentationDag::generateDotGraph() {
  std::string errorInfo;
  std::string functionName = getFunction().getName().str();
  std::string filename = "pathdag." + functionName + ".dot";

  DEBUG (dbgs() << "Writing '" << filename << "'...\n");
  raw_fd_ostream dotFile(filename.c_str(), errorInfo);

  if (!errorInfo.empty()) {
    errs() << "Error opening '" << filename.c_str() <<"' for writing!";
    errs() << "\n";
    return;
  }

  dotFile << "digraph " << functionName << " {\n";

  for( BLEdgeIterator edge = _edges.begin(), end = _edges.end();
       edge != end; edge++) {
    std::string sourceName = (*edge)->getSource()->getName();
    std::string targetName = (*edge)->getTarget()->getName();

    dotFile << "\t\"" << sourceName.c_str() << "\" -> \""
            << targetName.c_str() << "\" ";

    long inc = ((BLInstrumentationEdge*)(*edge))->getIncrement();

    switch( (*edge)->getType() ) {
    case BallLarusEdge::NORMAL:
      dotFile << "[label=" << inc << "] [color=black];\n";
      break;

    case BallLarusEdge::BACKEDGE:
      dotFile << "[color=cyan];\n";
      break;

    case BallLarusEdge::BACKEDGE_PHONY:
      dotFile << "[label=" << inc
              << "] [color=blue];\n";
      break;

    case BallLarusEdge::SPLITEDGE:
      dotFile << "[color=violet];\n";
      break;

    case BallLarusEdge::SPLITEDGE_PHONY:
      dotFile << "[label=" << inc << "] [color=red];\n";
      break;

    case BallLarusEdge::CALLEDGE_PHONY:
      dotFile << "[label=" << inc     << "] [color=green];\n";
      break;
    }
  }

  dotFile << "}\n";
}

// Allows subclasses to determine which type of Node is created.
// Override this method to produce subclasses of BallLarusNode if
// necessary. The destructor of BallLarusDag will call free on each pointer
// created.
BallLarusNode* BLInstrumentationDag::createNode(BasicBlock* BB) {
	return( new BLInstrumentationNode(BB) );
}

// Allows subclasses to determine which type of Edge is created.
// Override this method to produce subclasses of BallLarusEdge if
// necessary. The destructor of BallLarusDag will call free on each pointer
// created.
BallLarusEdge* BLInstrumentationDag::createEdge(BallLarusNode* source,
                                                BallLarusNode* target, unsigned edgeNumber) {
  // One can cast from BallLarusNode to BLInstrumentationNode since createNode
  // is overriden to produce BLInstrumentationNode.
  return( new BLInstrumentationEdge((BLInstrumentationNode*)source,
                                    (BLInstrumentationNode*)target) );
}

// Sets the Value corresponding to the pathNumber register, constant,
// or phinode.  Used by the instrumentation code to remember path
// number Values.
Value* BLInstrumentationNode::getStartingPathNumber(){
  return(_startingPathNumber);
}

// Sets the Value of the pathNumber.  Used by the instrumentation code.
void BLInstrumentationNode::setStartingPathNumber(Value* pathNumber) {
  DEBUG(dbgs() << "  SPN-" << getName() << " <-- " << (pathNumber ?
                                                       pathNumber->getName() :
                                                       "unused") << "\n");
  _startingPathNumber = pathNumber;
}

Value* BLInstrumentationNode::getEndingPathNumber(){
  return(_endingPathNumber);
}

void BLInstrumentationNode::setEndingPathNumber(Value* pathNumber) {
  DEBUG(dbgs() << "  EPN-" << getName() << " <-- "
               << (pathNumber ? pathNumber->getName() : "unused") << "\n");
  _endingPathNumber = pathNumber;
}

// Get the PHINode Instruction for this node.  Used by instrumentation
// code.
PHINode* BLInstrumentationNode::getPathPHI() {
  return(_pathPHI);
}

// Set the PHINode Instruction for this node.  Used by instrumentation
// code.
void BLInstrumentationNode::setPathPHI(PHINode* pathPHI) {
  _pathPHI = pathPHI;
}

// Removes the edge from the appropriate predecessor and successor
// lists.
void BLInstrumentationDag::unlinkEdge(BallLarusEdge* edge) {
  if(edge == getExitRootEdge())
    DEBUG(dbgs() << " Removing exit->root edge\n");

  edge->getSource()->removeSuccEdge(edge);
  edge->getTarget()->removePredEdge(edge);
}

// Makes an edge part of the spanning tree.
void BLInstrumentationDag::makeEdgeSpanning(BLInstrumentationEdge* edge) {
  edge->setIsInSpanningTree(true);
  _treeEdges.push_back(edge);
}

// Pushes initialization and calls itself recursively.
void BLInstrumentationDag::pushInitializationFromEdge(
  BLInstrumentationEdge* edge) {
  BallLarusNode* target;

  target = edge->getTarget();
  if( target->getNumberPredEdges() > 1 || target == getExit() ) {
    return;
  } else {
    for(BLEdgeIterator next = target->succBegin(),
          end = target->succEnd(); next != end; next++) {
      BLInstrumentationEdge* intoEdge = (BLInstrumentationEdge*) *next;

      // Skip split edges
      if (intoEdge->getType() == BallLarusEdge::SPLITEDGE)
        continue;

      intoEdge->setIncrement(intoEdge->getIncrement() +
                             edge->getIncrement());
      intoEdge->setIsInitialization(true);
      pushInitializationFromEdge(intoEdge);
    }

    edge->setIncrement(0);
    edge->setIsInitialization(false);
  }
}

// Pushes path counter increments up recursively.
void BLInstrumentationDag::pushCountersFromEdge(BLInstrumentationEdge* edge) {
  BallLarusNode* source;

  source = edge->getSource();
  if(source->getNumberSuccEdges() > 1 || source == getRoot()
     || edge->isInitialization()) {
    return;
  } else {
    for(BLEdgeIterator previous = source->predBegin(),
          end = source->predEnd(); previous != end; previous++) {
      BLInstrumentationEdge* fromEdge = (BLInstrumentationEdge*) *previous;

      // Skip split edges
      if (fromEdge->getType() == BallLarusEdge::SPLITEDGE)
        continue;

      fromEdge->setIncrement(fromEdge->getIncrement() +
                             edge->getIncrement());
      fromEdge->setIsCounterIncrement(true);
      pushCountersFromEdge(fromEdge);
    }

    edge->setIncrement(0);
    edge->setIsCounterIncrement(false);
  }
}

// Depth first algorithm for determining the chord increments.
void BLInstrumentationDag::calculateChordIncrementsDfs(long weight,
                                                       BallLarusNode* v, BallLarusEdge* e) {
  BLInstrumentationEdge* f;

  for(BLEdgeIterator treeEdge = _treeEdges.begin(),
        end = _treeEdges.end(); treeEdge != end; treeEdge++) {
    f = (BLInstrumentationEdge*) *treeEdge;
    if(e != f && v == f->getTarget()) {
      calculateChordIncrementsDfs(
        calculateChordIncrementsDir(e,f)*(weight) +
        f->getWeight(), f->getSource(), f);
    }
    if(e != f && v == f->getSource()) {
      calculateChordIncrementsDfs(
        calculateChordIncrementsDir(e,f)*(weight) +
        f->getWeight(), f->getTarget(), f);
    }
  }

  for(BLEdgeIterator chordEdge = _chordEdges.begin(),
        end = _chordEdges.end(); chordEdge != end; chordEdge++) {
    f = (BLInstrumentationEdge*) *chordEdge;
    if(v == f->getSource() || v == f->getTarget()) {
      f->setIncrement(f->getIncrement() +
                      calculateChordIncrementsDir(e,f)*weight);
    }
  }
}

// Determines the relative direction of two edges.
int BLInstrumentationDag::calculateChordIncrementsDir(BallLarusEdge* e,
                                                      BallLarusEdge* f) {
  if( e == NULL)
    return(1);
  else if(e->getSource() == f->getTarget()
          || e->getTarget() == f->getSource())
    return(1);

  return(-1);
}

