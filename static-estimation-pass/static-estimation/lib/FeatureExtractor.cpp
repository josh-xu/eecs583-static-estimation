#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Type.h"

#include <utility>
#include <string>

#include "FeatureExtractor.h"
#include "OpStatCounter.h"

FeatureExtractor::FeatureExtractor(std::set<Instruction*> pathInsts) {
    IRSet = pathInsts;
    return;
}

void FeatureExtractor::countHighLevelFeatures() {
    features.insert(featurepair("total_instructions", IRSet.size()));
}

void FeatureExtractor::countInstructionTypes() {
    OpStatCounter* counter = new OpStatCounter(IRSet);
    featuremap opmap = counter->get_opstats();
    // Merge maps
    features.insert(opmap.begin(), opmap.end());
    delete counter;
}

void FeatureExtractor::countTryCatch() {
    int n_tries = 0;
    int n_catches = 0;
    for (auto inst : IRSet) {
        std::string name = inst->getParent()->getName();
        // Check for catch blocks
        if (name.find("dispatch") == std::string::npos && name.find("catch") != std::string::npos) {
            n_catches++;
        }
        // Look for try blocks, but DON'T look for entry
        if (name.find("try") == 0) {
            n_tries++;
        }
    }
    features.insert(featurepair("n_tries", n_tries));
    features.insert(featurepair("n_catches", n_catches));
}

void FeatureExtractor::countEqualities() {
    int n_equalities = 0;
    for (auto inst : IRSet) {
        if (ICmpInst* icmp = dyn_cast<ICmpInst>(inst)) {
            if (icmp->isEquality())
                n_equalities++;
        }
    }
    features.insert(featurepair("n_equalities", n_equalities));
}

void FeatureExtractor::extractFeatures() {
    countHighLevelFeatures();
    countInstructionTypes();
    countEqualities();
    countTryCatch();
}
