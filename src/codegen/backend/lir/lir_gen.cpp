#include "codegen/backend/reg_ids.hpp"
#include "codegen/backend/visitors/mir_visitor.hpp"
#include "codegen/backend/lir/lir.hpp"
#include "codegen/backend/lir/lir_print.hpp"
#include "codegen/ir-gen/ir_types.h"
#include "errors/diagnostics.h"
#include <algorithm>
#include <iostream>
#include <ostream>
#include <stdint.h>
#include <variant>
#include <vector>
extern "C" {
    #include "codegen/ir-gen/mir.h"
}


struct LIRGen : MIRVisitor {
public:
    std::vector<LIRFunction>& lir_funcs;

    explicit LIRGen(BackendContext& ctx_, std::vector<LIRFunction>& lir_funcs_, std::ostream& out_)
    : ctx(ctx_), lir_funcs(lir_funcs_), out(out_) {
        curr_func = nullptr;
        curr_block = nullptr;
        inst_num = 0;
    }

    // Create new function.
    void pre_func(const IRFunction& f) override {
        lir_funcs.emplace_back(f.id, f.next_temp_id.id, f.fn_sym_id);
        curr_func = &lir_funcs.back();
        
        // Insert slot_sym table into ctx
        ctx.slot_syms.push_back(ctx.slot_to_symbol(f));
    }

    void post_func(const IRFunction& f) override {
        
    }

    // Create and push new block to current function.
    void pre_block(const IRBlock& b) override {
        curr_func->blocks.emplace_back(b.id);
        curr_block = &curr_func->blocks.back();

        // Construct successors
        VecView<BlockId> succs = VecView<BlockId>(&b.succs);
        for (auto& s: succs) {
            curr_block->succs.emplace_back(s);
        }
    }

    void post_block(const IRBlock& b) override {
        assert(curr_block->term.has_value());
    }

    void visit_inst(const IRInstruct& inst) override {
        switch (inst.type) {
            case IR_LOAD: create_load(inst); break;
            case IR_STORE: create_store(inst); break;
            case IR_HALT: create_ret(inst); break;
            case IR_BINOP: create_binop(inst); break;
            case IR_CMPOP: create_setcc(inst); break;
            case IR_JUMP: create_jump(inst); break;
            case IR_BRANCH_IF_ZERO: create_branch(inst); break;
            case IR_ARG: create_arg_get(inst); break;
            case IR_CALL: create_arg_puts_and_call(inst); break;
            default: out << "WARN: MIR -> LIR conversion for op not implemented." << std::endl; break;
        }
    }




private:
    BackendContext& ctx;
    LIRFunction* curr_func;
    LIRBlock* curr_block;
    // Instruction positions increase by 2 to allow insertion between instructions
    // Unique per function ??
    std::size_t inst_num;
    // todo: figure out how to pass a reference of this, or store it in ctx.
    std::vector<VRegInfo> vreg_info; // unused??
    std::ostream& out;


    // Inserts instruction into current block.
    void insert_instruction(LIRPayload pl) {
        curr_block->insts.emplace_back(pl, inst_num);
        // Incrementing by 2 so I can easily add spill/restore instructions
        // after regalloc (I hope).
        inst_num += 2;
    }

    // Inserts terminator into separate terminator instruction.
    void insert_terminator(LIRPayload term_pl) {
        curr_block->term = LIRInstruct(term_pl, inst_num);
        inst_num += 2;
    }

    // Creates new vreg in current function.
    VRegId create_vreg(LIRFunction& f) {
        return VRegId{f.next_vreg++};
    }

    // Creates const op, and returns the vreg it's stored in.
    VRegId create_const(const uint64_t imm) {
        VRegId vreg = create_vreg(*curr_func);
        insert_instruction(LIRConst(vreg, imm));
        return vreg;
    }

    // TODO: create all ops, and learn some error handling here.
    void create_load(const IRInstruct& inst) {
        const Load& load = inst.payload.load_payload;

        compare_slot_id_to_max(load.src);

        VRegId vreg = VRegId{load.dst.id};
        // todo: insert into vreginfo.
        LIRLoad lir_load = LIRLoad(load.src, vreg);

        insert_instruction(lir_load);
    }

    void create_store(const IRInstruct& inst) {
        const Store& store = inst.payload.store_payload;

        compare_slot_id_to_max(store.dst);

        // For myself: returns a lambda, the [&] lets me use
        // outer scoped variables.
        LIRStore lir_store =
            (store.src.value_kind == IRVAL_TEMP)
                ? LIRStore(store.dst, VRegId{store.src.value_id.temp_id.id})
                : [&] {
                    VRegId vreg = create_const(store.src.value_id.imm);
                    return LIRStore(store.dst, vreg);
                }();

        insert_instruction(lir_store);
    }

    // Function stuff (arguments and calls)

    void create_arg_get(const IRInstruct& inst) {
        const Arg& arg = inst.payload.arg_pl;

        LIRArgGet lir_arg = LIRArgGet(VRegId{arg.dst.id}, arg.arg_id);
        insert_instruction(lir_arg);
    }

    void create_arg_put(ArgId arg_id, VRegId vreg) {
        LIRArgPut lir_arg_put = LIRArgPut(arg_id, vreg);
        insert_instruction(lir_arg_put);
    }

    void create_arg_puts_and_call(const IRInstruct& inst) {
        const Call& call = inst.payload.call_pl;
        const VecView<IRValue> args = ctx.fn_args(call);

        // Get vector of args and create 'arg_put' instructions.
        for (int i = 0; i < args.size(); i++) {
            const IRValue& arg = args[i];

            VRegId vreg{SIZE_MAX};
            if (arg.value_kind == IRVAL_IMM) {
                // materialize?
                vreg = create_const(arg.value_id.imm);
            }
            else if (arg.value_kind == IRVAL_TEMP) {
                vreg.id = arg.value_id.temp_id.id;
            }
            create_arg_put(i, vreg);
        }

        // Because I don't support overflow/spill args YET.
        assert(args.size() < 8);

        // Create 'call' instruction
        LIRCall lir_call = LIRCall(VRegId{call.dst.id}, call.fn_sym_id, args.size());
        insert_instruction(lir_call);
    }

    // Binary operators

    // Lowers LHS/RHS, then creates add operation.
    void create_add_sub(BinOp& binop) {
        // if LHS of add is negative, flip LHS/RHS
        if (binop.kind == BIN_ADD) {
            if (binop.lhs.value_kind == IRVAL_IMM && binop.lhs.value_id.imm < 0) {
                std::swap(binop.lhs, binop.rhs);
            }
        }
        // If RHS imm is negative, switch instructions.
        if (binop.rhs.value_kind == IRVAL_IMM) {
            if (binop.rhs.value_id.imm < 0) {
                // Flip op and take abs(imm)
                flip_binop(&binop);
                binop.rhs.value_id.imm = abs(binop.rhs.value_id.imm);
            }
        }
        Operand lhs = lower_alu_operand(binop.lhs);
        Operand rhs = lower_alu_operand(binop.rhs);
        BinOpKind binop_kind = binop.kind;
        LIRBinOp lir_binop = LIRBinOp(binop_kind, VRegId{binop.dst.id}, lhs, rhs);
        insert_instruction(lir_binop);
    }
    
    // Lowers LHS/RHS to vregs, then creates mul/div op.
    void create_mul_usdiv(const BinOp& binop) {
        Operand lhs = lower_mul_operand(binop.lhs);
        Operand rhs = lower_mul_operand(binop.rhs);
        BinOpKind binop_kind = binop.kind;
        LIRBinOp lir_binop = LIRBinOp(binop_kind, VRegId{binop.dst.id}, lhs, rhs);
        insert_instruction(lir_binop);
    }


    void create_udiv(const BinOp& binop) {
        std::cerr << "ICE: UDiv operation not implemented yet." << std::endl;
    }

    void create_binop(const IRInstruct& inst) {
        BinOp binop = inst.payload.binop_pl;
        switch (inst.payload.binop_pl.kind) {
            case BIN_ADD:
            case BIN_SUB: create_add_sub(binop); break;
            case BIN_MUL: 
            case BIN_SDIV: 
            case BIN_UDIV: create_mul_usdiv(binop); break;
            case BIN_ERROR:
            {
                std::cerr << "ICE: Binary operator not found." << std::endl;
                break;
            }
        }
    }

    void create_setcc(const IRInstruct& inst) {
        Cmp cmp = inst.payload.cmp_pl;
        // TODO** Working on this. I need to understand how ARM handles comparisons/branches.
        // Opted to just lower it during emission or something IDK.
        LIRSetCC lir_setcc = LIRSetCC(VRegId{cmp.dst.id}, cmp.kind, 
            lower_alu_operand(cmp.lhs), lower_alu_operand(cmp.rhs));
        insert_instruction(lir_setcc);
    }

    // Terminators

    void create_branch(const IRInstruct& inst) {
        Branch br = inst.payload.br_pl;
        VRegId vreg;
        if (br.cmp.value_kind == IRVAL_IMM) {
            vreg = create_const(br.cmp.value_id.imm);
        }
        else if (br.cmp.value_kind == IRVAL_TEMP) {
            vreg = VRegId{br.cmp.value_id.temp_id.id};
        }
        LIRBranch lir_br{vreg, br.non_zero, br.zero};
        insert_terminator(lir_br);
    }

    void create_jump(const IRInstruct& inst) {
        Jump jump = inst.payload.jump_pl;
        insert_terminator(LIRJump{jump.jump_to});
    }

    void create_ret(const IRInstruct& inst) {
        const Halt& halt = inst.payload.halt_payload;

        LIRRet lir_ret = 
            (halt.code.value_kind == IRVAL_TEMP)
                ? LIRRet(VRegId{halt.code.value_id.temp_id.id})
                : [&] {
                    VRegId vreg = create_const(halt.code.value_id.imm);
                    return LIRRet(vreg);
                }();
        insert_terminator(lir_ret);
    }

    // Binary operator helpers

    // Checks if the immediate would fit inside of the add/sub
    // operation in ARM.
    bool fits_add_imm(int64_t imm) {
        int64_t a = abs(imm);
        return (a <= 4095); // || // TODO: add back shifted case; add shift op.
            //((a & 0xfff) == 0 && (a >> 12) <= 4095);
    }

    // Changes add-> sub or sub-> add.
    void flip_binop(BinOp* binop) {
        if (binop->kind == BIN_ADD) {
            binop->kind = BIN_SUB;
        }
        else if (binop->kind == BIN_SUB) {
            binop->kind = BIN_ADD;
        }
    }

    // todo: could unify these later.
    // Checks if 'imm' fits, if not emit const op
    // Currently for add/sub/cmp ops.
    Operand lower_alu_operand(const IRValue& val) {
        if (val.value_kind == IRVAL_TEMP) {
            return Operand{VRegId{val.value_id.temp_id.id}};
        }
        else if (val.value_kind == IRVAL_IMM) {
            if (!fits_add_imm(val.value_id.imm)) {
                VRegId vreg = create_const(val.value_id.imm);
                return Operand{vreg};
            }
            return Operand{val.value_id.imm};
        }
        assert(false);
    }

    // Mul only accepts registers, so convert if necessary.
    Operand lower_mul_operand(const IRValue& val) {
        if (val.value_kind == IRVAL_TEMP) {
            return Operand{VRegId{val.value_id.temp_id.id}};
        }
        else if (val.value_kind == IRVAL_IMM) {
            VRegId vreg = create_const(val.value_id.imm);
            return Operand{vreg};
        }
        assert(false);
    }
    
    // Compare passed in slot ID to current found max ID. Tells us how 
    // many slots exist in this function.
    void compare_slot_id_to_max(SlotId slot_id) {
        curr_func->max_slot_id = std::max( curr_func->max_slot_id, slot_id.id);
    }


};


void lower_mir_to_lir(BackendContext& ctx, bool debug) {
    LIRGen gen(ctx, ctx.lir_funcs, std::cout);
    walk_mir_linear(ctx, gen);
    if (debug) print_lir(gen.lir_funcs, ctx, std::cout);
}