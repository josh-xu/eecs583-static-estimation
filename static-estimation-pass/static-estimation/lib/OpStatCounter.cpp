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
    other = 0;
    branch = 0;
    all = 0;
}

float OpStatCounter::get_percentage(int val) {
    if (all == 0)
        return 0;

    return val/float(all);
}

featuremap OpStatCounter::get_opstats() {
    featuremap opstat_map;
    // Generate totals for insts
    run_counts();
    // Calculate percentages. We're doing this to try and normalize features and thus make life
    // easier for when we run classification
    opstat_map.insert(featurepair("percent_intops", get_percentage(integerALU)));
    opstat_map.insert(featurepair("percent_floatops", get_percentage(floatingALU)));
    opstat_map.insert(featurepair("percent_loads", get_percentage(loads)));
    opstat_map.insert(featurepair("percent_stores", get_percentage(stores)));
    opstat_map.insert(featurepair("percent_memops", get_percentage(memory)));
    opstat_map.insert(featurepair("percent_branchops", get_percentage(branch)));
    opstat_map.insert(featurepair("percent_otherops", get_percentage(other)));
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
    
            default:
                other++;
                break;
        }
    }
}
