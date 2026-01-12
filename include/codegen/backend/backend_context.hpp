#pragma once
#include "codegen/backend/lir/lir.hpp"
#include "codegen/backend/regalloc/regalloc_analysis.hpp"
#include "codegen/backend/frame-layout/frame_layout.hpp"
#include "vector/vec_view.hpp"
#include <vector>
extern "C" {
    #include "common.h"
    #include "codegen/ir-gen/mir.h"
}

// std::visit helper (visit payloads mostly)
template <class... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
};
template <class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;




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

    // todo: maybe make these private with accessors?
    // depends on if they need to be mutated.
    // All indexed by FuncId.
    std::vector<LIRFunction> lir_funcs;
    std::vector<LivenessInfo> liveness;
    // Location info mapping vreg -> (preg|slot).
    std::vector<std::vector<Location>> locs;
    std::vector<std::optional<FrameInfo>> frame_info;
    std::vector<TableView<Symbol>> slot_syms;

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

    bool op_is_imm(Operand op) {
        bool is_imm;
        std::visit(overloaded{
            [&](const Reg& reg) {
                is_imm = false;
            },
            [&](const uint64_t) {
                is_imm = true;
            }
        }, op);
        return is_imm;
    }

    // Regalloc stuff

    const std::vector<Location>& locs_by_func(FuncId id) {
        return locs[id.id];
    }

    const std::size_t vreg_to_preg(FuncId fid, VRegId vreg) {
        if (locs_by_func(fid)[vreg.id].kind == LOC_PREG)
            return locs_by_func(fid)[vreg.id].id;
        return SIZE_MAX;
    }

    // Frame info stuff
    inline FrameInfo* get_frame_info_ptr(FuncId fid) {
        auto& opt = frame_info[fid.id];
        assert(opt.has_value() && "FrameInfo not initialized for function");
        return &*opt;
    }

    inline const LIRInstruct& get_terminator(
    const std::optional<LIRInstruct>& term)
    {
        assert(term.has_value() && "Block has no terminator");
        return *term;
    }


private:
    const IRModule& mod;
    
};




