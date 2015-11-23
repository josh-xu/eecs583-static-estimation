#ifndef OPSTAT_H 
#define OPSTAT_H

#include "llvm/Pass.h"
#include "llvm/IR/Function.h"

#include "FeatureExtractor.h"

#include <set>

using namespace llvm;

class OpStatCounter {
    featuremap opstats;
    std::set<Instruction*> IRSet;

    long integerALU;
    long floatingALU;
    long memory;
    long loads;
    long stores;
    long other;
    long branch;
    long all;

    void run_counts();

    public:
        OpStatCounter(std::set<Instruction*> pathInsts);
        featuremap get_opstats();
};

#endif
