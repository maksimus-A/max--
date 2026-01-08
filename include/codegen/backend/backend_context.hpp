#pragma once
#include "codegen/backend/lir/lir.hpp"
#include "vector/vec_view.hpp"
#include <vector>
extern "C" {
    #include "common.h"
    #include "codegen/ir-gen/mir.h"
}

struct LivenessInfo {
    // Indexed by BlockId.
    std::vector<BitSet> use;
    std::vector<BitSet> def;
    std::vector<BitSet> in;
    std::vector<BitSet> out;
};



class BackendContext {
public:
    BackendContext(IRModule& mod_)
        :mod(mod_) {}

    std::vector<LIRFunction> lir_funcs;
    std::vector<LivenessInfo> liveness;

    // Returns the vector of MIR functions.
    VecView<IRFunction> mir_functions() { return VecView<IRFunction>(mod.funcs); }

    // Returns vector of MIR blocks of current function.
    VecView<IRBlock> mir_blocks(const IRFunction& f) {
        return VecView<IRBlock>(&f.blocks);
    }

    VecView<IRInstruct> mir_insts(const IRBlock& b) {
        return VecView<IRInstruct>(&b.instructions);
    }

    // Table: symbol_id -> symbol
    TableView<Symbol> symbol_res() const {
        return TableView<Symbol>(*mod.name_resolution);
    }

    // Table: slot_id -> symbol (per function)
    TableView<Symbol> slot_to_symbol(const IRFunction& f) const{
        return TableView<Symbol>(f.slot_sym);
    }

    // Maybe store the slot sym ptr table?

    // LIR stuff

    std::vector<LIRBlock>& lir_blocks(LIRFunction& f) {
        return f.blocks;
    }

    std::vector<LIRInstruct>& lir_insts(LIRBlock& b) {
        return b.insts;
    }

private:
    const IRModule& mod;
    // Sooo just so I don't forget:
    // This vector is indexed by FuncId.
    // Each vector inside of liveness is indexed by
    // the blockId inside a function.
    
};


