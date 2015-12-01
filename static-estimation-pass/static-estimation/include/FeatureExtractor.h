#ifndef FTEXT_H
#define FTEXT_H

#include "llvm/Pass.h"
#include "llvm/IR/Function.h"

#include <vector>
#include <map>
#include <string>

using namespace llvm;

typedef std::map<std::string, float> featuremap;
typedef std::pair<std::string, float> featurepair;

class FeatureExtractor {
    featuremap features;
    // We'll keep both the instruction list and BB path in memory
    // in an attempt to not throw away information. IRSet gets built
    // during object construction and won't change, however
    std::vector<Instruction*> InstPath;
    std::vector<BasicBlock*> BBPath;
    
    public:
        FeatureExtractor(std::vector<BasicBlock*> path);
        void extractFeatures();
        void countTryCatch();
        void countInstructionTypes();
        void countHighLevelFeatures();
        void countEqualities();
        void countLocalGlobalVars();
        void countCallInfo();

        std::string getFeaturesCSV();
        std::string getFeaturesCSVNames();

        featuremap getFeatures() {
            return features;
        }
};

#endif
