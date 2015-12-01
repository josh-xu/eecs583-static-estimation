#ifndef FTEXT_H
#define FTEXT_H

#include "llvm/Pass.h"
#include "llvm/IR/Function.h"

#include <set>
#include <map>
#include <string>

using namespace llvm;

typedef std::map<std::string, float> featuremap;
typedef std::pair<std::string, float> featurepair;

class FeatureExtractor {
    featuremap features;
    std::set<Instruction*> IRSet;
    
    public:
        FeatureExtractor(std::set<Instruction*> pathInsts);
        void extractFeatures();
        void countTryCatch();
        void countInstructionTypes();
        void countHighLevelFeatures();
        void countEqualities();
        void countLocalGlobalVars();
        void countCallInfo();

        featuremap getFeatures() {
            return features;
        }
};

#endif
