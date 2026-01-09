#include "codegen/backend/backend_context.hpp"
#include "codegen/backend/lir/lir.hpp"
#include "codegen/backend/visitors/lir_visitor.hpp"


/**
live = OUT[B] (done)

(Rest needs to be implemented; nothing done yet.)
For any v ∈ live, treat it as already “open” with open_end = block_end_pos
Walk instructions backwards:
Let p be the position of this instruction (or “before” it)
For each d ∈ defs(I):
If d is currently live/open, then you close its range at p (because the value before the def is dead)
Remove d from live
For each u ∈ uses(I):
If u is not currently live/open, then you open a new range starting at p
Add u to live
*/
struct RegAllocAnalysis: LIRVisitor {
public:
    explicit RegAllocAnalysis(BackendContext& ctx_, bool debug_, std::ostream& out_)
        :ctx(ctx_), debug(debug_), out(out_), live() {
            curr_func = nullptr;
            curr_block = nullptr;
            curr_info = nullptr;
        }

    void pre_func(const LIRFunction& f) override {
        curr_info = &ctx.liveness[f.id.id];
        curr_info->max_vreg_id = f.next_vreg-1;
        curr_info->intervals.resize(curr_info->max_vreg_id+1);

        curr_func = &f;
    }

    void pre_block(const LIRBlock& b) override {
        curr_block = &b;

        // Set live = OUT[B]
        live = curr_info->out[b.id.id];
    }

    // Uhhhhh
    void visit(const LIRLoad& load) override {
        
    }
    void visit(const LIRStore& store) override {
        
    }
    void visit(const LIRBinOp& binop) override {
        // Check whether LHS/RHS are imm/vregs. If Imm skip.
        // If vregs, do normal checking.
        if (std::holds_alternative<VRegId>(binop.lhs)) {
            VRegId lhs = std::get<VRegId>(binop.lhs);
            
        }
        if (std::holds_alternative<VRegId>(binop.rhs)) {
            VRegId rhs = std::get<VRegId>(binop.rhs);
            
        }

        note_def(binop.dst);
    }
    void visit(const LIRConst& con) override {
        note_def(con.dst);
    }
    void visit(const LIRRet& ret) override {
        note_use(ret.id);
    }

private:
    BackendContext& ctx;
    bool debug;
    std::ostream& out;

    const LIRFunction* curr_func;
    const LIRBlock* curr_block;
    LivenessInfo* curr_info;

    BitSet live;

    void def()

};