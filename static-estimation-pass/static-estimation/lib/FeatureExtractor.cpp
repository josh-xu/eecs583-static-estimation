#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"

#include <utility>
#include <string>

#include "FeatureExtractor.h"

FeatureExtractor::FeatureExtractor(std::set<Instruction*> pathInsts) {
    IRSet = pathInsts;
    errs() << "OUTPUT!!!\n";
    return;
}

void FeatureExtractor::extractFeatures() {
    features.insert(std::pair<std::string, float>("total_instructions", IRSet.size()));
}
