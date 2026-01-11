// Printing LIR!
#include "codegen/backend/lir/lir.hpp"
#include "common.hpp"

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

static void print_instruction(const LIRInstruct& inst, std::ostream& out_) {
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
}

static void print_term(const std::optional<LIRTerm>& term, std::ostream& out) {
    if (std::holds_alternative<LIRRet>(term->pl)) {
        LIRRet ret = std::get<LIRRet>(term->pl);
        out << term->inst_num << ": ret ";
        print_reg(ret.id, out);
        out << std::endl;
    }
}

void print_lir(const std::vector<LIRFunction>& lir_funcs_, std::ostream& out_) {
    for (auto& f: lir_funcs_) {
        out_ << "function_" << f.id.id << ":\n";
        for (auto& b: f.blocks) {
            out_ << "  block_" << b.id.id << ":\n";
            for (auto& inst: b.insts) {
                out_ << "    " << inst.inst_num << ": ";
                print_instruction(inst, out_);
            }
            out_ << "    ";
            print_term(b.term, out_);
        }
    }
}