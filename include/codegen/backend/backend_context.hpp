#pragma once
#include "codegen/backend/lir/lir.hpp"
#include "vector/vec_view.hpp"
#include <vector>
extern "C" {
    #include "common.h"
    #include "codegen/ir-gen/mir.h"
}

// For variable range creation/analysis
// All unique per function.
using Pos = std::size_t;
struct Range { Pos start, end; }; // [start, end)
struct Interval { std::vector<Range> ranges; /* maybe also vreg id */ };


struct LivenessInfo {
    // todo: consider moving these elsewhere. they can be discarded after intervals are constructed.
    // Indexed by BlockId. Unique per function.
    std::vector<BitSet> use, def, in, out;

    // indexed by vreg id
    std::vector<Interval> intervals; 
    std::size_t max_vreg_id;
};

class BackendContext {
public:
    BackendContext(IRModule& mod_)
        :mod(mod_) {}

    std::vector<LIRFunction> lir_funcs;
    std::vector<LivenessInfo> liveness; // Indexed by func_id

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


