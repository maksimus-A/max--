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
        out << "\n";
    }
    
    void visit_inst(const LIRInstruct& inst) override {
        std::visit([&](auto& payload) {
            visit(payload);
        }, inst.pl);
    }

    // Computes use/def per block.
    void visit(const LIRLoad& load) override {
        note_def(get_vreg(load.dst));
    }
    void visit(const LIRStore& store) override {
        note_use(get_vreg(store.src));
    }
    void visit(const LIRBinOp& binop) override {
        // Check whether LHS/RHS are imm/vregs. If Imm skip.
        // If vregs, do normal checking.
        if (std::holds_alternative<Reg>(binop.lhs)) {
            Reg lhs = std::get<Reg>(binop.lhs);
            VRegId vreg = get_vreg(lhs);
            note_use(vreg);
        }
        if (std::holds_alternative<Reg>(binop.rhs)) {
            Reg rhs = std::get<Reg>(binop.rhs);
            VRegId vreg = get_vreg(rhs);
            note_use(vreg);
        }

        note_def(get_vreg(binop.dst));
    }
    void visit(const LIRConst& con) override {
        note_def(get_vreg(con.dst));
    }
    void visit(const LIRRet& ret) override {
        note_use(get_vreg(ret.id));
    }
    void visit(const LIRSetCC& setcc) {
        note_def(get_vreg(setcc.dst));

        if (std::holds_alternative<Reg>(setcc.lhs)) {
            Reg lhs = std::get<Reg>(setcc.lhs);
            VRegId vreg = get_vreg(lhs);
            note_use(vreg);
        }
        if (std::holds_alternative<Reg>(setcc.rhs)) {
            Reg rhs = std::get<Reg>(setcc.rhs);
            VRegId vreg = get_vreg(rhs);
            note_use(vreg);
        }
    }
    void visit(const LIRBranch& br) {
        note_use(get_vreg(br.cmp));
    }
    void visit(const LIRJump& jump) {
        // No uses or definitions here.
    }
    // Function stuff
    void visit(const LIRArgGet& arg_get) {
        note_def(get_vreg(arg_get.dst));
    }
    void visit(const LIRArgPut& arg_put) {
        note_use(get_vreg(arg_put.src)); 
    }
    void visit(const LIRCall& call) {
        note_def(get_vreg(call.dst));
    }

    /*----- IN/OUT PHASE: ------*/
    /*
    Equations (per block):
    new_out[B] = ⋃ in[S] for all successors S of B
    new_in[B] = use[B] ∪ (new_out[B] − def[B])
    Worklist behavior:
    Put blocks in a stack/queue initially
    Pop B, recompute new_in/new_out.
    If either changed vs the stored in/out, overwrite them and push pred[B].
    */
    void construct_in_out() {
        for (auto& f: ctx.lir_funcs) {
            // Set livenessinfo
            curr_info = &ctx.liveness[f.id.id];

            std::vector<char> in_worklist(f.blocks.size(), 0);
            // Reverse post order.
            block_stack.clear();
            create_block_stack(f);
            std::reverse(block_stack.begin(), block_stack.end());

            for (auto id : block_stack) in_worklist[id.id] = 1;

            // when you push:
            auto enqueue = [&](BlockId id) {
                if (!in_worklist[id.id]) {
                    in_worklist[id.id] = 1;
                    block_stack.push_back(id);
                }
            };

            while (!block_stack.empty()) {

                // when you pop:
                BlockId bid = block_stack.back();
                block_stack.pop_back();
                in_worklist[bid.id] = 0;

                const LIRBlock& curr_b = get_block(bid, f);

                // compute new_out
                BitSet new_out(f.next_vreg);
                for (auto& succ : curr_b.succs) {
                    new_out.or_assign(curr_info->in[succ.id]);
                }

                // compute new_in = use | (new_out & ~def)
                BitSet temp = new_out.and_not_with(curr_info->def[curr_b.id.id]);

                BitSet new_in = curr_info->use[curr_b.id.id];
                new_in.or_assign(temp);

                // if changed, write back + enqueue preds
                if (!new_out.equals_with(curr_info->out[curr_b.id.id]) ||
                    !new_in.equals_with(curr_info->in[curr_b.id.id])) {

                    curr_info->out[curr_b.id.id] = new_out;
                    curr_info->in[curr_b.id.id]  = new_in;

                    for (auto& pred : curr_b.preds) {
                        enqueue(pred);
                    }
                }

                if (debug) print_in_out(curr_b);
            }

            // Debug: print finalized in/out.
            
        }
    }

    void print_in_out(const LIRBlock& b) {
        out << "Block " << b.id.id << ": IN:" << std::endl;
        curr_info->in[b.id.id].print_bitset();

        out << "Block 0" << b.id.id << ": OUT:" << std::endl;
        curr_info->out[b.id.id].print_bitset();
        out << "\n";
    }

    /*------ VARIABLE LIVENESS PHASE: ------*/

    // Constructs intervals determining vreg liveness.
    // TODO: Maybe make a new struct/pass for this part later.
    void construct_vreg_liveness() {
        for (auto& f: ctx.lir_funcs) {
            for (auto& b: f.blocks) {
                
            }
        }
    }


private:
    BackendContext& ctx;
    bool debug;
    std::ostream& out;

    const LIRFunction* curr_func;
    const LIRBlock* curr_block;
    LivenessInfo* curr_info;

    // For IN/OUT analysis
    std::vector<BlockId> block_stack;


    

     /*------ USE/DEF PHASE: ------*/
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

    /*------ IN/OUT PHASE: ------*/
    void create_block_stack(LIRFunction& f) {
        block_stack.clear();

        std::vector<char> visited(f.blocks.size(), 0);
        LIRBlock& block = f.blocks[0];
        add_succ_blocks(block, f, visited);
    }

    void add_succ_blocks(const LIRBlock& b, LIRFunction& f, std::vector<char>& visited) {
        if (visited[b.id.id]) return;
        visited[b.id.id] = true;

        for (auto& block_id: b.succs) {
            const LIRBlock& succ_block = get_block(block_id, f);
            add_succ_blocks(succ_block, f, visited);
        }
        block_stack.push_back(b.id);
    }

    const LIRBlock& get_block(const BlockId block_id, const LIRFunction& f) {
        return f.blocks[block_id.id];
    }

    /*------ VARIABLE LIVENESS PHASE: ------*/
    // This phase calculates live ranges of vregs.


};

void liveness_analysis(BackendContext& ctx, bool debug) {
    LivenessAnalysis liveness_anal(ctx, debug, std::cout);
    walk_lir_linear(ctx, liveness_anal);

    // In/out calculation
    liveness_anal.construct_in_out();

    // Liveness calculation (important one!)
    liveness_anal.construct_vreg_liveness();
}