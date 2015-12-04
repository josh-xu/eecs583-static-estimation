#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Type.h"

#include <vector>
#include <utility>
#include <string>
#include <sstream>
#include <iomanip>

#include "FeatureExtractor.h"
#include "OpStatCounter.h"

// I don't actually know this value so I'm choosing an arbitrary value
#define MAX_OPCODE 100

FeatureExtractor::FeatureExtractor(std::vector<BasicBlock*> path) {
    BBPath = path;
    for (auto bb : path) {
        for (BasicBlock::iterator i = bb->begin(), e = bb->end(); i != e; ++i) {
            InstPath.push_back(&*i);
        }
    }
    return;
}

void FeatureExtractor::countHighLevelFeatures() {
    features.insert(featurepair("total_instructions", InstPath.size()));
}

// For a given BB, create a map of (opcode -> # of times for this opcode)
instvector FeatureExtractor::getBlockInstVec(BasicBlock* BB) {
    instvector bbVector;
    for (BasicBlock::iterator i = BB->begin(), e = BB->end(); i != e; ++i) {
        Instruction* inst = &*i;
        bbVector[inst->getOpcode()]++;
    }
    return bbVector;
}

// Extract fixed length features for each basic block, one BB on each line
// The features correspond to how many times each opcode was found in the BB
std::string FeatureExtractor::getFeaturesLSTM() {
    std::ostringstream line;
    for (auto bb : BBPath) {
        std::string sep = "";
        instvector bbVector = FeatureExtractor::getBlockInstVec(bb);
        for (int i=0; i<MAX_OPCODE; i++) {
            unsigned n_count = 0;
            if (bbVector.find(i) != bbVector.end())
                n_count = bbVector[i];

            line << sep << n_count;
            sep = ",";
        }
        line << "\n";
    }
    line << "\n";
    return line.str();
}

std::string FeatureExtractor::getFeaturesCSVNames() {
    std::ostringstream csvLine;
    std::string sep = "";
    for (auto i = features.begin(), e = features.end(); i != e; ++i) {
        csvLine << sep << i->first;
        sep = ",";
    }
    csvLine << "\n";
    return csvLine.str();
}

std::string FeatureExtractor::getFeaturesCSV() {
    std::ostringstream csvLine;
    std::string sep = "";
    for (auto i = features.begin(), e = features.end(); i != e; ++i) {
        csvLine << sep << std::setw(4) << i->second;
        sep = ",";
    }
    csvLine << "\n";
    return csvLine.str();
}

void FeatureExtractor::countInstructionTypes() {
    OpStatCounter* counter = new OpStatCounter(InstPath);
    featuremap opmap = counter->get_opstats();
    // Merge maps
    features.insert(opmap.begin(), opmap.end());
    delete counter;
}

void FeatureExtractor::countLocalGlobalVars() {
    int n_locals = 0;
    int n_globals = 0;
    for (auto inst: InstPath) {
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
    for (auto bb : BBPath) {
        std::string name = bb->getName();
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
    for (auto inst : InstPath) {
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
    for (auto inst : InstPath) {
        if (CallInst *callInst = dyn_cast<CallInst>(inst)) {
            n_function_calls++;
            n_params += callInst->getNumArgOperands();
        }
    }
    features.insert(featurepair("n_function_calls", n_function_calls));
 
    float avg_args;
    if (n_function_calls > 0)
        avg_args = n_params/float(n_function_calls);
    else
        avg_args = 0;
    features.insert(featurepair("n_avg_args_per_call", avg_args));
}

void FeatureExtractor::extractFeatures() {
    countHighLevelFeatures();
    countInstructionTypes();
    countEqualities();
    countTryCatch();
    countLocalGlobalVars();
    countCallInfo();
}
