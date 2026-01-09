#include "codegen/backend/visitors/mir_visitor.hpp"
#include "codegen/backend/lir/lir.hpp"
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
        lir_funcs.emplace_back(f.id, f.next_temp_id.id);
        curr_func = &lir_funcs.back();
    }

    void post_func(const IRFunction& f) override {
        
    }

    // Create and push new block to current function.
    void pre_block(const IRBlock& b) override {
        curr_func->blocks.emplace_back(b.id);
        curr_block = &curr_func->blocks.back();
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
        }
    }

    // Printing LIR!
    void print_lir() {
        for (auto& f: lir_funcs) {
            out << "function_" << f.id.id << ":\n";
            for (auto& b: f.blocks) {
                out << "  block_" << b.id.id << ":\n";
                for (auto& inst: b.insts) {
                    out << "    ";
                    print_instruction(inst);
                }
                out << "    ";
                print_term(b.term);
            }
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
    std::vector<VRegInfo> vreg_info;
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

        VRegId vreg = VRegId{load.dst.id};
        // todo: insert into vreginfo.
        LIRLoad lir_load = LIRLoad(load.src, vreg);

        insert_instruction(lir_load);
    }

    void create_store(const IRInstruct& inst) {
        const Store& store = inst.payload.store_payload;

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
        Operand lhs = lower_add_operand(binop.lhs);
        Operand rhs = lower_add_operand(binop.rhs);
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

    // Binary operator helpers

    // Checks if the immediate would fit inside of the add/sub
    // operation in ARM.
    bool fits_add_imm(int64_t imm) {
        int64_t a = abs(imm);
        return (a <= 4095) ||
            ((a & 0xfff) == 0 && (a >> 12) <= 4095);
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
    Operand lower_add_operand(const IRValue& val) {
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

    void print_bin_op(const LIRBinOp& binop) {
        switch (binop.bin_op_kind) {
            case BIN_ADD: out << "add "; break;
            case BIN_SUB: out << "sub "; break;
            case BIN_MUL: out << "mul "; break;
            case BIN_SDIV: out << "sdiv "; break;
            case BIN_UDIV: out << "udiv "; break;
            case BIN_ERROR: out << "ERROR_OP "; break;
        }
        out << "v" << binop.dst.id << ", ";

        if (std::holds_alternative<int64_t>(binop.lhs)) {
            int64_t lhs = std::get<int64_t>(binop.lhs);
            out << "#" << lhs << ", ";
        }
        if (std::holds_alternative<VRegId>(binop.lhs)) {
            VRegId lhs = std::get<VRegId>(binop.lhs);
            out << "v" << lhs.id << ", ";
        }
        if (std::holds_alternative<int64_t>(binop.rhs)) {
            int64_t rhs = std::get<int64_t>(binop.rhs);
            out << "#" << rhs << std::endl;
        }
        if (std::holds_alternative<VRegId>(binop.rhs)) {
            VRegId rhs = std::get<VRegId>(binop.rhs);
            out << "v" << rhs.id << std::endl;
        }
    }

    void print_instruction(const LIRInstruct& inst) {
        if (std::holds_alternative<LIRStore>(inst.pl)) {
            LIRStore store = std::get<LIRStore>(inst.pl);
            out << "store slot(" << store.dst.id << "), v" << store.src.id << std::endl;
        }
        if (std::holds_alternative<LIRLoad>(inst.pl)) {
            LIRLoad load = std::get<LIRLoad>(inst.pl);
            out << "load v" << load.dst.id << ", slot(" << load.src.id << ")" << std::endl;
        }
        if (std::holds_alternative<LIRConst>(inst.pl)) {
            LIRConst const_ = std::get<LIRConst>(inst.pl);
            out << "const v" << const_.dst.id << ", #" << const_.src << std::endl;
        }
        if (std::holds_alternative<LIRBinOp>(inst.pl)) {
            print_bin_op(std::get<LIRBinOp>(inst.pl));
        }
    }

    void print_term(const std::optional<LIRTerm>& term) {
        if (std::holds_alternative<LIRRet>(term->pl)) {
            LIRRet ret = std::get<LIRRet>(term->pl);
            out << "ret v" << ret.id.id << std::endl;
        }
    }
};


void lower_mir_to_lir(BackendContext& ctx, bool debug) {
    LIRGen gen(ctx, ctx.lir_funcs, std::cout);
    walk_mir_linear(ctx, gen);
    if (debug) gen.print_lir();
}