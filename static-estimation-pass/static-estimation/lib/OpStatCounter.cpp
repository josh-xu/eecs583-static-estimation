#include "llvm/Support/raw_ostream.h"

#include "OpStatCounter.h"

OpStatCounter::OpStatCounter(std::vector<Instruction*> pathInsts) {
    IRSet = pathInsts;

    // Various counters
    integerALU = 0;
    floatingALU = 0;
    memory = 0;
    loads = 0;
    stores = 0;
    alloca = 0;
    branch = 0;
    compares = 0;
    equality_checks = 0;
    relational_checks = 0;
    other = 0;
    all = 0;
}

featuremap OpStatCounter::get_opstats() {
    featuremap opstat_map;
    // Generate totals for insts
    run_counts();
    // Calculate percentages. We're doing this to try and normalize features and thus make life
    // easier for when we run classification
    opstat_map.insert(featurepair("percent_intops", integerALU/float(all)));
    opstat_map.insert(featurepair("percent_floatops", floatingALU/float(all)));
    opstat_map.insert(featurepair("percent_memops", memory/float(all)));
    opstat_map.insert(featurepair("percent_loads", loads/float(all)));
    opstat_map.insert(featurepair("percent_stores", stores/float(all)));
    opstat_map.insert(featurepair("percent_alloca", alloca/float(all)));
    opstat_map.insert(featurepair("percent_branchops", branch/float(all)));
    opstat_map.insert(featurepair("percent_compares", compares/float(all)));
    opstat_map.insert(featurepair("percent_equality_checks", equality_checks/float(all)));
    opstat_map.insert(featurepair("percent_equality_checks", relational_checks/float(all)));
    opstat_map.insert(featurepair("percent_otherops", other/float(all)));
    return opstat_map;
}

void OpStatCounter::run_counts() {
    for (auto inst : IRSet) {
        all++;
        switch(inst->getOpcode()) {
            // Integer ALU
            case Instruction::Add :
            case Instruction::Sub :
            case Instruction::Mul :
            case Instruction::UDiv :
            case Instruction::SDiv :
            case Instruction::URem :
            case Instruction::Shl :
            case Instruction::LShr :
            case Instruction::AShr :
            case Instruction::And :
            case Instruction::Or :
            case Instruction::Xor :
            case Instruction::ICmp :
            case Instruction::SRem :
                integerALU++;
                break;

            // Floating-point ALU
            case Instruction::FAdd :
            case Instruction::FSub :
            case Instruction::FMul :
            case Instruction::FDiv :
            case Instruction::FRem :
            case Instruction::FCmp :
                floatingALU++;
                break;

            // Memory operation
            case Instruction::Load :
                loads++;
                memory++;
                break;

            case Instruction::Store :
                stores++;
                memory++;
                break;

            case Instruction::Alloca :
                alloca++;
                memory++;
                break;

            case Instruction::GetElementPtr :
            case Instruction::Fence :
            case Instruction::AtomicCmpXchg:
            case Instruction::AtomicRMW:
                memory++;
                break;

            // Branch instructions
            case Instruction::Br :
            case Instruction::Switch :
            case Instruction::IndirectBr :
                branch++;
                break;
    
            case Instruction::ICmpInst :
                compares++;
                ICmpInst * cmp = dyn_cast<ICmpInst>inst;
                if(cmp->isEquality()){
                    equality_checks++;
                }
                else{
                    relational_checks++;
                }
                break;

            case Instruction::FCmpInst :
                compares++;
                FCmpInst * cmp = dyn_cast<FCmpInst>inst;
                if(cmp->isEquality()){
                    equality_checks++;
                }
                else{
                    relational_checks++;
                }
                break;

            default:
                other++;
                break;
        }
    }
}
