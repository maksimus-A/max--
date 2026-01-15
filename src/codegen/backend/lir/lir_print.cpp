// Printing LIR!
#include "codegen/backend/lir/lir.hpp"
#include "codegen/ir-gen/mir.h"
#include "common.hpp"

static std::string block_label(const BlockId& bid, const FuncId& fid) {
    return "block_" + std::to_string(fid.id) + std::to_string(bid.id);
}

static std::string func_label(const FuncId& fid) {
    return "function_" + std::to_string(fid.id);
}

static void print_reg(const Reg& r, std::ostream& out) {
    if (alt<VRegId>(r.id)) out << "v" << get<VRegId>(r.id).id;
    else if (alt<PRegId>(r.id)) out << "p" << get<PRegId>(r.id).id;
    else assert(false);
}

static void print_bin_op(const LIRBinOp& binop, std::ostream& out) {
    switch (binop.bin_op_kind) {
        case BIN_ADD: out << "add "; break;
        case BIN_SUB: out << "sub "; break;
        case BIN_MUL: out << "mul "; break;
        case BIN_SDIV: out << "sdiv "; break;
        case BIN_UDIV: out << "udiv "; break;
        case BIN_ERROR: out << "ERROR_OP "; break;
    }
    print_reg(binop.dst, out);
    out << ", ";

    if (alt<int64_t>(binop.lhs)) {
        int64_t lhs = std::get<int64_t>(binop.lhs);
        out << "#" << lhs << ", ";
    }
    if (alt<Reg>(binop.lhs)) {
        Reg lhs = std::get<Reg>(binop.lhs);
        print_reg(lhs, out);
        out << ", ";
    }
    if (alt<int64_t>(binop.rhs)) {
        int64_t rhs = std::get<int64_t>(binop.rhs);
        out << "#" << rhs << std::endl;
    }
    if (alt<Reg>(binop.rhs)) {
        Reg rhs = std::get<Reg>(binop.rhs);
        print_reg(rhs, out);
        out << std::endl;
    }
}

static void print_cmp(const CmpKind& kind, std::ostream& out) {
    switch (kind) {
        case CMP_EQ: out << "EQ"; break;
        case CMP_NEQ: out << "NEQ"; break;
        case CMP_GT: out << "GT"; break;
        case CMP_LT: out << "LT"; break;
        case CMP_ERR: out << "CMP_ERR"; break; 
    }
}

static void print_instruction(const LIRInstruct& inst, const BlockId& bid, const FuncId& fid, std::ostream& out_) {
    if (alt<LIRStore>(inst.pl)) {
        LIRStore store = std::get<LIRStore>(inst.pl);
        out_ << "store slot(" << store.dst.id << "), ";
        print_reg(store.src, out_);
        out_ << std::endl;
    }
    if (alt<LIRLoad>(inst.pl)) {
        LIRLoad load = std::get<LIRLoad>(inst.pl);
        out_ << "load ";
        print_reg(load.dst, out_);
        out_ << ", slot(" << load.src.id << ")" << std::endl;
    }
    if (alt<LIRConst>(inst.pl)) {
        LIRConst const_ = std::get<LIRConst>(inst.pl);
        out_ << "const ";
        print_reg(const_.dst, out_);
        out_ << ", #" << const_.src << std::endl;
    }
    if (alt<LIRBinOp>(inst.pl)) {
        print_bin_op(std::get<LIRBinOp>(inst.pl), out_);
    }
    if (alt<LIRSetCC>(inst.pl)) {
        LIRSetCC setcc = get<LIRSetCC>(inst.pl);
        out_ << "setcc ";
        print_reg(setcc.dst, out_);
        out_ << ", ";
        print_cmp(setcc.kind, out_);
        out_ << ", ";
        
        if (alt<int64_t>(setcc.lhs)) {
            int64_t lhs = std::get<int64_t>(setcc.lhs);
            out_ << "#" << lhs << ", ";
        }
        if (alt<Reg>(setcc.lhs)) {
            Reg lhs = std::get<Reg>(setcc.lhs);
            print_reg(lhs, out_);
            out_ << ", ";
        }
        if (alt<int64_t>(setcc.rhs)) {
            int64_t rhs = std::get<int64_t>(setcc.rhs);
            out_ << "#" << rhs << std::endl;
        }
        if (alt<Reg>(setcc.rhs)) {
            Reg rhs = std::get<Reg>(setcc.rhs);
            print_reg(rhs, out_);
            out_ << std::endl;
        }
    }
}

static void print_term(const std::optional<LIRTerm>& term, const BlockId& bid, const FuncId& fid, std::ostream& out) {
    if (std::holds_alternative<LIRRet>(term->pl)) {
        LIRRet ret = std::get<LIRRet>(term->pl);
        out << "ret ";
        print_reg(ret.id, out);
    }
    if (alt<LIRBranch>(term->pl)) {
        const LIRBranch br = get<LIRBranch>(term->pl);
        out << "branch ";
        print_reg(br.cmp, out);
        out << ", " << block_label(br.non_zero, fid) << ", " << block_label(br.zero, fid);
    }
    if (alt<LIRJump>(term->pl)) {
        const LIRJump jump = get<LIRJump>(term->pl);
        out << "jump " << block_label(jump.jump_to, fid);
    }
    out << std::endl;
}



void print_lir(const std::vector<LIRFunction>& lir_funcs_, std::ostream& out_) {
    for (auto& f: lir_funcs_) {
        out_ << "\nLIR GENERATION: " << "\n";
        out_ << func_label(f.id) << ":\n";
        for (auto& b: f.blocks) {
            if (b.id.id != 0)
                out_ <<  block_label(b.id, f.id) << ":\n";

            for (auto& inst: b.insts) {
                out_ << inst.inst_num << ": " << "    " ;
                print_instruction(inst, b.id, f.id, out_);
            }
            out_ << b.term->inst_num << ": ";
            out_ << "    ";
            print_term(b.term, b.id, f.id, out_);

            out_ << "\n";
        }
    }
}