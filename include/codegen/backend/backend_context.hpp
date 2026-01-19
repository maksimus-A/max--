#pragma once
#include "codegen/backend/lir/lir.hpp"
#include "codegen/backend/regalloc/regalloc_analysis.hpp"
#include "codegen/backend/frame-layout/frame_layout.hpp"
#include "vector/vec_view.hpp"
#include <vector>
#include <string_view>
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
    const IRModule& mod;
    const Source& source_file;

    
    BackendContext(IRModule& mod_, Source& source_file_)
        :mod(mod_), source_file(source_file_) {}

    // todo: maybe make these private with accessors?
    // depends on if they need to be mutated.
    // All indexed by FuncId.
    std::vector<LIRFunction> lir_funcs;
    std::vector<LivenessInfo> liveness;
    // Location info mapping vreg -> (preg|slot).
    std::vector<std::vector<Location>> locs;
    std::vector<std::optional<FrameInfo>> frame_info;
    std::vector<TableView<Symbol>> slot_syms;
    // Maps FuncId -> used_callee_regs vector
    // Set after regalloc (used inside Regalloc struct)
    std::vector<std::vector<int>> fn_used_callee_regs;

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
        return TableView<Symbol>(*mod.name_resolution); // name res is a bad name.
    }

    // Table: slot_id -> symbol (per function)
    TableView<Symbol> slot_to_symbol(const IRFunction& f) const{
        return TableView<Symbol>(f.slot_sym);
    }

    // Table: symbolId -> slotID (per function)
    TableView<SlotId> symbol_to_slot(const IRFunction& f) const{
        return TableView<SlotId>(f.sym_slot);
    }

    // Function arguments from a function call.
    VecView<IRValue> fn_args(const Call& call) const {
        return VecView<IRValue>(&call.args);
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

    // Print file slice in cpp
    void print_file_slice(char* start, size_t length, std::ostream& out) const {
        for (size_t i = 0; i < length; i++) {
            out << *(start + i);
        }
    }

    // Grabs actual string name (start pointer) from span in buffer.
    char* start_of_name(SrcSpan span) const {
        return &source_file.buffer[span.start];
    }

    // Grabs string name from symbol ID.
    char* start_of_name(size_t sym_id) {
        TableView<Symbol> syms = TableView<Symbol>(*mod.name_resolution);
        Symbol sym = *syms[sym_id];

        return start_of_name(sym.symbol_span);
    }

    const Symbol* get_symbol(size_t sym_id) {
        TableView<Symbol> syms = TableView<Symbol>(*mod.name_resolution);
        return syms[sym_id];
    }

    // Prints symbol name from name resolution (which is symid->Symbol*)
    const void print_symbol_name(size_t sym_id, std::ostream& out) const {
        TableView<Symbol> syms = TableView<Symbol>(*mod.name_resolution);
        Symbol sym = *syms[sym_id];

        char* start = start_of_name(sym.symbol_span);
        print_file_slice(start, sym.symbol_span.length, out);
    }

    bool func_is_main(size_t sym_id) {
        TableView<Symbol> syms = TableView<Symbol>(*mod.name_resolution);
        Symbol sym = *syms[sym_id];

        const char* start = start_of_name(sym.symbol_span);
        std::string_view name(start, sym.symbol_span.length);

        return name == "main";
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

    
};




