#pragma once
#include "codegen/backend/regalloc/regalloc_analysis.hpp"
#include "codegen/backend/reg_ids.hpp"
#include <cstddef>
#include <optional>
#include <variant>
#include <vector>
extern "C" {
    #include "ast/parser/ast.h"
    #include "codegen/ir-gen/mir.h"
}

class BackendContext;


// todo: make table in BackendContext that stores vregid->VRegInfo
struct VRegInfo { // virtual register (for temps)
    std::size_t size;
    BuiltInType type;
};

/* ------ Instruction payload helpers ------*/

/* ------ Instruction payloads ------*/
struct LIRStore {
    explicit LIRStore(SlotId dst_, VRegId src_)
        : dst(dst_), src(src_) {}
    explicit LIRStore(SlotId dst_, PRegId src_)
        : dst(dst_), src(src_) {}
    SlotId dst;
    Reg src;
};

struct LIRLoad { 
    explicit LIRLoad(SlotId src_, VRegId dst_)
        : src(src_), dst(dst_) {}
    explicit LIRLoad(SlotId src_, PRegId dst_)
        : src(src_), dst(dst_) {}
    Reg dst;
    SlotId src;
};

// Materializes immediate into register.
struct LIRConst {
    explicit LIRConst(VRegId dst_, int64_t src_)
        : dst(dst_), src(src_) {}
    Reg dst;
    int64_t src;
};

// TODO: Refactor to 'return' once functions exist.
struct LIRRet { // comes from exit(0), placeholder for 'return;'
    explicit LIRRet(VRegId code_)
        : id(code_) {}
    Reg id;
};

// Binary operators

using Operand = std::variant<Reg, int64_t>;

struct LIRBinOp {
    explicit LIRBinOp(BinOpKind bin_op_kind_, VRegId dst_, Operand lhs_, Operand rhs_)
        : bin_op_kind(bin_op_kind_), dst(dst_), lhs(lhs_), rhs(rhs_) {}
    BinOpKind bin_op_kind;
    Reg dst;
    Operand lhs;
    Operand rhs;
};

using LIRPayload = std::variant<LIRStore, LIRLoad, LIRRet, 
                            LIRConst, LIRBinOp>;


struct LIRInstruct {
    explicit LIRInstruct(LIRPayload pl_, std::size_t inst_num_)
        : pl(pl_), inst_num(inst_num_) {}
    LIRPayload pl;
    std::size_t inst_num;
};

using LIRTerm = LIRInstruct;

struct LIRBlock {
    explicit LIRBlock(BlockId id_)
        : id(id_) {}

    std::vector<LIRInstruct> insts;
    BlockId id;

    std::optional<LIRTerm> term;

    // For regalloc
    // TODO*: actually build these in CFGGen pass!
    // currently only ret terminator exists so there's nothing to do.
    std::vector<BlockId> preds;
    std::vector<BlockId> succs;

};

struct LIRFunction {

    explicit LIRFunction(FuncId id_, size_t next_temp_id)
        :id(id_), next_vreg(next_temp_id) {
            entry = BlockId{0}; // todo: consider some kind of wrapper to init this
            max_slot_id = 0;
        }

    std::vector<LIRBlock> blocks;
    std::size_t next_vreg; // this can also tell max_vregs per function.
    std::size_t max_slot_id; // For register allocation later.
    BlockId entry;

    FuncId id;
};

void lower_mir_to_lir(BackendContext& ctx, bool debug);



