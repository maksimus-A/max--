#pragma once
#include "codegen/backend/regalloc/regalloc_analysis.hpp"
#include "codegen/backend/reg_ids.hpp"
#include "vector/vec_view.hpp"
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

// Function stuff

// Grab argument from call regs.
struct LIRArgGet {

    explicit LIRArgGet(VRegId dst_, ArgId src_)
        : dst(dst_), src(src_) {}

    Reg dst;
    ArgId src;
};

// Put arguments into call regs.
struct LIRArgPut {
    explicit LIRArgPut(ArgId dst_, VRegId src_)
        : dst(dst_), src(src_) {}

    ArgId dst;
    Reg src;
};

// Call a function.
struct LIRCall {
    explicit LIRCall(VRegId dst_, size_t fn_sym_id_, size_t argc_)
        : dst(dst_), fn_sym_id(fn_sym_id_), argc(argc_) {}

    Reg dst;
    size_t fn_sym_id;
    size_t argc; // # of args being passed
};

// Materializes immediate into register.
struct LIRConst {
    explicit LIRConst(VRegId dst_, int64_t src_)
        : dst(dst_), src(src_) {}
    Reg dst;
    int64_t src;
};

// MIR Halt == LIR Ret
struct LIRRet {
    explicit LIRRet(VRegId code_)
        : has_value(true), id(code_) {}

    explicit LIRRet()
        : has_value(false), id(VRegId{SIZE_MAX}) {}

    bool has_value;
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

// This should just perform subtraction on the two operands.
// Theoretically sets flags CZNV or something.
// But for now i'll just perma materialize into a temp.
// Stands for 'Set Condition Code'
struct LIRSetCC {
    explicit LIRSetCC(VRegId dst_, CmpKind kind_, Operand lhs_, Operand rhs_)
        : dst(dst_), kind(kind_), lhs(lhs_), rhs(rhs_) {}

    Reg dst;
    CmpKind kind;
    Operand lhs;
    Operand rhs;
};

struct LIRBranch {
    explicit LIRBranch(VRegId cmp_, BlockId non_zero_, BlockId zero_)
        : cmp(cmp_), non_zero(non_zero_), zero(zero_) {}
    Reg cmp;
    BlockId non_zero;
    BlockId zero;
};

struct LIRJump {
    BlockId jump_to;
};



using LIRPayload = std::variant<LIRStore, LIRLoad, LIRRet, 
                            LIRConst, LIRBinOp, LIRSetCC,
                            LIRBranch, LIRJump, LIRArgGet,
                            LIRArgPut, LIRCall>;


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
    std::vector<BlockId> preds;
    std::vector<BlockId> succs;

};

struct LIRFunction {

    explicit LIRFunction(FuncId id_, size_t next_temp_id, size_t fn_sym_id_)
        :id(id_), next_vreg(next_temp_id), fn_sym_id(fn_sym_id_) {
            entry = BlockId{0}; // todo: consider some kind of wrapper to init this
            max_slot_id = 0;
        }

    std::vector<LIRBlock> blocks;
    std::size_t next_vreg; // this can also tell max_vregs per function.
    std::size_t max_slot_id; // For register allocation later.
    BlockId entry;

    FuncId id;
    // Honestly this shouldn't be so tied to functions but it's my only way of 
    // printing the funciton name right now.
    // TODO: make a FuncId -> Symbol* table in frontend.
    size_t fn_sym_id;
};

void lower_mir_to_lir(BackendContext& ctx, bool debug);



