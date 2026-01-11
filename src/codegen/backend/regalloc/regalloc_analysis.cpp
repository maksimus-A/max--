#include "codegen/backend/backend_context.hpp"
#include "codegen/backend/lir/lir.hpp"
#include "codegen/backend/visitors/lir_visitor.hpp"
#include "codegen/backend/regalloc/regalloc_analysis.hpp"


struct RegAllocAnalysis: LIRVisitor {
public:
    explicit RegAllocAnalysis(BackendContext& ctx_, bool debug_, std::ostream& out_)
        :ctx(ctx_), debug(debug_), out(out_), live() {
            curr_func = nullptr;
            curr_block = nullptr;
            curr_info = nullptr;
            // Defined during 2nd pass.
            regalloc = nullptr;
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

    /*------ LINEAR SCAN REGISTER ALLOCATION PASS ------*/

    // TODO: If intervals are actually split, this will do the wrong thing.
    // Need to sort per interval too.
    // it's a minimal change; each interval has associated vreg,
    // so just unpack 'Intervals' if you want to change it.
    void linear_scan_regalloc() {
        for (auto& f: ctx.lir_funcs) {
            curr_info = &ctx.liveness[f.id.id];
            std::vector<Interval>& unhandled = curr_info->intervals;
            RegAllocInfo regalloc = RegAllocInfo(ARM_FREE_REGS, f.max_slot_id, f.next_vreg);

            // Sort the intervals by their start of first range.
            std::sort(unhandled.begin(), unhandled.end(),
            [](const Interval& a, const Interval& b) {
                return a.start() < b.start();
            });

            for (auto& interval: unhandled) {
                // TODO: Relies on the fact there's only 1 range here.
                // If you want multiple range support modify this.
                Range* r = &interval.ranges.front();

                remove_inactive_ranges(regalloc, r);
                if (pregs_available(regalloc)) {
                    allocate_preg(regalloc, r);
                    regalloc.active.push_back(r);
                    sort_active_by_end(regalloc.active);
                }
                else {
                    allocate_slot(regalloc, r);
                }
            }

            // Push to ctx
            ctx.locs.push_back(regalloc.locs);
            if (debug) print_preg_allocations(regalloc);
        }
    }

    void print_preg_allocations(const RegAllocInfo& regalloc) {
        for (int i = 0; i < regalloc.locs.size(); i++) {
            const Location& loc = regalloc.locs[i];
            if (loc.kind == LOC_PREG) {
                out << "v" << i << ": p" << loc.id << std::endl;
            }
            else if (loc.kind == LOC_SLOT) {
                out << "v" << i << ": slot(" << loc.id << ")" << std::endl;
            }
        }
    }

    /*------ ADDING SPILLED VREG TO INST PASS ------*/
    void add_spilled_to_insts() {
            /*
        Idea: run through loc list per function.
        if kind == LOC_SLOT,
            find instruction # where we first spill.
            insert 'load into scratch' before
            insert 'store into slot' after.
        That's kind of it.
        Append new insts to LIRBlock
        */
        for (auto& f: ctx.lir_funcs) {
            const std::vector<Location>& locs = ctx.locs_by_func(f.id);

            for (auto& b: ctx.lir_blocks(f)) {
                std::vector<LIRInstruct> insts;

                for (const auto& i: ctx.lir_insts(b)) {

                    LIRInstruct inst = i;

                    for (const auto& vreg: uses(i)) {
                        if (locs[vreg.id].kind == LOC_PREG) {
                            
                        }
                        else if (locs[vreg.id].kind == LOC_SLOT) {
                            
                        }
                    }
                    for (const auto& vreg: defs(i)) {
                        if (locs[vreg.id].kind == LOC_PREG) {
                            
                        }
                        else if (locs[vreg.id].kind == LOC_SLOT) {
                            
                        }
                    }
                }
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

    // 'Liveness' information during block scans (to construct intervals)
    BitSet live;

    RegAllocInfo* regalloc;

    // Returns set of defined vregs in an instruction.
    std::vector<VRegId> defs(const LIRInstruct& inst) {
        std::vector<VRegId> defined;

        std::visit(overloaded{
            [&](const LIRLoad& load) {
                defined.push_back(get_vreg(load.dst));
            },
            [&](const LIRConst& cons) {
                defined.push_back(get_vreg(cons.dst));
            },
            [&](const LIRBinOp& binop) {
                defined.push_back(get_vreg(binop.dst));
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
                if (!ctx.op_is_imm(binop.lhs)) {
                    Reg reg = std::get<Reg>(binop.lhs);
                    used.push_back(get_vreg(reg));
                }
                if (!ctx.op_is_imm(binop.rhs)) {
                    Reg reg = std::get<Reg>(binop.rhs);
                    used.push_back(get_vreg(reg));
                }
            },
            [&](const LIRStore& store) {
                used.push_back(get_vreg(store.src));
            },
            [&](const LIRRet& ret) {
                used.push_back(get_vreg(ret.id));
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
            interval.ranges.back().vreg = vreg;
        }
        else {
            assert(false && "insert_start_range called without open range");
        }
        // TODO: Emit diagnostics error.
    }
 
    // Insert the 'end' var in an interval [start, end)
    // Needs to create a new interval first.
    // TODO: Merge intervals since my linear scan algo is lazy. Must-do for now.
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
        out << std::endl;
    }

    void print_range(Range range) {
        out << "[" << range.start << ", " << range.end << ")";
    }

    // LINEAR SCAN REGISTER ALLOCATION PASS
       void sort_active_by_end(std::vector<Range*>& active) {
        std::sort(active.begin(), active.end(),
        [](const Range* a, const Range* b)
                { return a->end < b->end; });
    }

    void remove_inactive_ranges(RegAllocInfo& regalloc, Range* r) {
        for (auto it = regalloc.active.begin(); it != regalloc.active.end();) {
            if ((*it)->end < r->start) {
                // Clears the preg currently live associated to the range passed in.
                Location preg_loc = regalloc.locs[(*it)->vreg.id];
                if (preg_loc.kind == LOC_SLOT) continue;

                std::size_t preg = preg_loc.id;
                regalloc.free_regs.clear(preg);

                // Remove this range from 'active' ranges.
                it = regalloc.active.erase(it);
            }
            else {
                ++it;
            }
        }
    }

    // 0 -> inactive preg, 1 -> active preg
    bool pregs_available(RegAllocInfo& regalloc) {
        BitSet filled = BitSet(regalloc.free_regs.num_bits);
        filled.set_ones_bit_vec();

        if (regalloc.free_regs.equals_with(filled)) return false;
        return true;
    }

    void allocate_preg(RegAllocInfo& regalloc, Range* r) {
        int free_reg = regalloc.free_regs.get_first_zero_position();
        if (free_reg == -1) {
            // TODO: Add diagnostic error
            out << "Preg was supposed to have available slot but none found.";
            return;
        }

        // Set location of vreg/preg
        r->assigned_preg.id = (size_t)free_reg;
        regalloc.locs[r->vreg.id] = Location(LOC_PREG, r->assigned_preg.id);
        // Set preg to 'used'
        regalloc.free_regs.set((size_t)free_reg);
    }

    void allocate_slot(RegAllocInfo& regalloc, Range* r) {
        Range* victim = get_latest_end(regalloc);
        if (victim->end > r->end) {
            spill_to_slot(regalloc, victim);
            PRegId victim_preg = victim->assigned_preg;
            // Give current range 'victim's preg
            r->assigned_preg = victim_preg;
            victim->assigned_preg.id = SIZE_MAX;
            // Assign r's loc to preg
            regalloc.locs[r->vreg.id] = Location(LOC_PREG, victim_preg.id);
            // Add r to active set of ranges
            regalloc.active.push_back(r);
            sort_active_by_end(regalloc.active);
            // Remove 'victim' from active set
            auto it = std::find(regalloc.active.begin(), regalloc.active.end(), victim);
            if (it != regalloc.active.end()) regalloc.active.erase(it);

        }
        else {
            spill_to_slot(regalloc, r);
        }
    }

    void spill_to_slot(RegAllocInfo& regalloc, Range* r) {
        Location loc = Location(LOC_SLOT, regalloc.slot_counter++);
        regalloc.locs[r->vreg.id] = loc;
    }

    Range* get_latest_end(RegAllocInfo& regalloc) {
        assert(!regalloc.active.empty());
        Range* max_r = regalloc.active[0];

        for (auto& r: regalloc.active) {
            if (r->end > max_r->end) {
                max_r = r;
            }
        }
        return max_r;
    }

};

void regalloc(BackendContext& ctx, bool debug) {
    RegAllocAnalysis regalloc(ctx, debug, std::cout);
    walk_lir_backwards_insts_linear(ctx, regalloc);
    
    // Now actually allocate physical registers using linear scan.
    regalloc.linear_scan_regalloc();
}