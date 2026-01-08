#include "codegen/backend/lir/lir.hpp"
#include "codegen/backend/visitors/lir_visitor.hpp"


struct LivenessAnalysis: LIRVisitor {
public:
    explicit LivenessAnalysis(BackendContext& ctx_, bool debug_, std::ostream& out_)
        :ctx(ctx_), debug(debug_), out(out_) {
            curr_func = nullptr;
            curr_block = nullptr;
            curr_info = nullptr;
            ctx.liveness.resize(ctx.lir_funcs.size());
        }

    /*------ USE/DEF PHASE: ------*/
    void pre_func(const LIRFunction& f) override {
        curr_info = &ctx.liveness[f.id.id];
        // Init each vec (size # blocks) to bitset equal to # of vregs.
        curr_info->def.resize(f.blocks.size(), BitSet(f.next_vreg));
        curr_info->use.resize(f.blocks.size(), BitSet(f.next_vreg));
        curr_info->in.resize(f.blocks.size(), BitSet(f.next_vreg));
        curr_info->out.resize(f.blocks.size(), BitSet(f.next_vreg));

        curr_func = &f;
    }

    void pre_block(const LIRBlock& b) override {
        curr_block = &b;
    }

    void post_block(const LIRBlock& b) override {
        if (!debug) return;

        out << "Block " << curr_block->id.id << ": DEF:" << std::endl;
        curr_info->def[curr_block->id.id].print_bitset();
        out << "Block " << curr_block->id.id << ": USE:" << std::endl;
        curr_info->use[curr_block->id.id].print_bitset();
    }
    
    void visit_inst(const LIRInstruct& inst) override {
        std::visit([&](auto& payload) {
            visit(payload);
        }, inst.pl);
    }

    // Computes use/def per block.
    void visit(const LIRLoad& load) override {
        note_def(load.dst);
    }
    void visit(const LIRStore& store) override {
        note_use(store.src);
    }
    void visit(const LIRBinOp& binop) override {
        // Check whether LHS/RHS are imm/vregs. If Imm skip.
        // If vregs, do normal checking.
        if (std::holds_alternative<VRegId>(binop.lhs)) {
            VRegId lhs = std::get<VRegId>(binop.lhs);
            note_use(lhs);
        }
        if (std::holds_alternative<VRegId>(binop.rhs)) {
            VRegId rhs = std::get<VRegId>(binop.rhs);
            note_use(rhs);
        }

        note_def(binop.dst);
    }
    void visit(const LIRConst& con) override {
        note_def(con.dst);
    }
    void visit(const LIRRet& ret) override {
        note_use(ret.id);
    }

    /*----- IN/OUT PHASE: ------*/


private:
    BackendContext& ctx;
    bool debug;
    std::ostream& out;

    const LIRFunction* curr_func;
    const LIRBlock* curr_block;
    LivenessInfo* curr_info;

    void note_def(VRegId vreg) {
        size_t id = vreg.id;
        curr_info->def[curr_block->id.id].set(id);
    }

    // Sets use of vreg only if it wasn't previously defined in the block.
    void note_use(VRegId vreg) {
        size_t id = vreg.id;
        if (!curr_info->def[curr_block->id.id].test(id)) {
            curr_info->use[curr_block->id.id].set(id);
        }
    }
};

void liveness_analysis(BackendContext& ctx, bool debug) {
    LivenessAnalysis liveness_anal(ctx, debug, std::cout);
    walk_lir_linear(ctx, liveness_anal);
}