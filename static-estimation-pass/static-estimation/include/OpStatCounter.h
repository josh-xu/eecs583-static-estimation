#ifndef OPSTAT_H 
#define OPSTAT_H

#include "llvm/Pass.h"
#include "llvm/IR/Function.h"

#include "FeatureExtractor.h"

#include <vector>

using namespace llvm;

class OpStatCounter {
    featuremap opstats;
    std::vector<Instruction*> IRSet;

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
        float get_percentage(int val);
        OpStatCounter(std::vector<Instruction*> pathInsts);
        featuremap get_opstats();
};

#endif
