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

    void post_block(const LIRBlock& b) override {
        if (!debug) return;

        print_live_intervals();
    }

    void visit_inst(const LIRInstruct& inst) override {
        // Range = [start, end)
        for (auto& d: defs(inst)) {
            // Check if vreg 'd' is in live set
            if (live.test(d.id)) {
                // 'start' the range (from top-down)
                insert_start_range(d, inst.inst_num);
            }
            live.clear(d.id);
        }
        for (auto& u: uses(inst)) {
            // If vreg isn't already in a range, 'end' the range.
            if (!live.test(u.id)) {
                insert_end_range(u, inst.inst_num + 1);
            }
            live.set(u.id);
        }
    }

    // Uhhhhh
    /*
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
    }*/

private:
    BackendContext& ctx;
    bool debug;
    std::ostream& out;

    const LIRFunction* curr_func;
    const LIRBlock* curr_block;
    LivenessInfo* curr_info;

    BitSet live;

    // Returns set of defined vregs in an instruction.
    std::vector<VRegId> defs(const LIRInstruct& inst) {
        std::vector<VRegId> defined;

        std::visit(overloaded{
            [&](const LIRLoad& load) {
                defined.push_back(load.dst);
            },
            [&](const LIRConst& cons) {
                defined.push_back(cons.dst);
            },
            [&](const LIRBinOp& binop) {
                defined.push_back(binop.dst);
            },
            [&](auto const&) {
                // no defs
            }
        }, inst.pl);

        return defined;
    }

    // Returns set of used vregs in an instruction.
    std::vector<VRegId> uses(const LIRInstruct& inst) {
        std::vector<VRegId> used;

        std::visit(overloaded{
            [&](const LIRBinOp& binop) {
                if (!ctx.op_is_imm(binop.lhs))
                    used.push_back(std::get<VRegId>(binop.lhs));
                if (!ctx.op_is_imm(binop.rhs))
                    used.push_back(std::get<VRegId>(binop.rhs));
            },
            [&](const LIRStore& store) {
                used.push_back(store.src);
            },
            [&](const LIRRet& ret) {
                used.push_back(ret.id);
            },
            [&](auto const&) {
                // no uses
            }
        }, inst.pl);

        return used;
    }

    // Insert the 'start' var in an interval [start, end)
    void insert_start_range(VRegId vreg, size_t inst_num) {
        Interval& interval = curr_info->intervals[vreg.id];
        assert(!interval.ranges.empty());
        if (interval.ranges.back().start == SIZE_MAX) {
            interval.ranges.back().start = inst_num;
        }
        else {
            assert(false && "insert_start_range called without open range");
        }
        // TODO: Emit diagnostics error.
    }
 
    // Insert the 'end' var in an interval [start, end)
    // Needs to create a new interval first.
    // TODO: Merge intervals if necessary.
    void insert_end_range(VRegId vreg, size_t inst_num) {
        Interval& interval = curr_info->intervals[vreg.id];
        if (!interval.ranges.empty()) {
             if (interval.ranges.back().start == SIZE_MAX) return;
        }

        Range range = Range(SIZE_MAX, inst_num);
        interval.ranges.push_back(range);
    }

    // Debug printing
    void print_live_intervals() {
        out << "Block " << curr_block->id.id << ": " << std::endl;
        for (int i = 0; i < curr_info->intervals.size(); i++) {
            const auto& interval = curr_info->intervals[i];

            out << "v" << i << ": ";
            for (int j = 0; j < interval.ranges.size(); j++) {
                print_range(interval.ranges[j]);
                out << ", ";
            }
            out << std::endl;
        }
    }

    void print_range(Range range) {
        out << "[" << range.start << ", " << range.end << ")";
    }

};

void regalloc(BackendContext& ctx, bool debug) {
    RegAllocAnalysis regalloc(ctx, debug, std::cout);
    walk_lir_backwards_insts_linear(ctx, regalloc);
}