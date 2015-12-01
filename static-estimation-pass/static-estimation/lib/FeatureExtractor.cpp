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

void FeatureExtractor::countLocalGlobalVars() {
    int n_locals = 0;
    int n_globals = 0;
    for (auto inst: IRSet) {
        if (isa<LoadInst>(inst)) {
            Value* operand = inst->getOperand(0);
            if (isa<GlobalVariable>(operand)) {
                n_globals++;
            }
            else {
                n_locals++;
            }
        }
    }
    features.insert(featurepair("n_globals", n_globals));
    features.insert(featurepair("n_locals", n_locals));
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

void FeatureExtractor::countCallInfo() {
    int n_function_calls = 0;
    int n_params = 0;
    for (auto inst : IRSet) {
        if (CallInst *callInst = dyn_cast<CallInst>(inst)) {
            n_function_calls++;
            n_params += callInst->getNumArgOperands();
        }
    }
    features.insert(featurepair("n_function_calls", n_function_calls));
    features.insert(featurepair("n_avg_args_per_call", n_params/float(n_function_calls)));
}

void FeatureExtractor::extractFeatures() {
    countHighLevelFeatures();
    countInstructionTypes();
    countEqualities();
    countTryCatch();
    countLocalGlobalVars();
    countCallInfo();
}
