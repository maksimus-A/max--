#include "codegen/backend/backend_context.hpp"
#include "codegen/backend/lir/lir.hpp"
#include "codegen/backend/lir/lir_print.hpp"
#include "codegen/backend/visitors/lir_visitor.hpp"
#include "codegen/backend/regalloc/regalloc_analysis.hpp"
#include "codegen/backend/reg_ids.hpp"
#include "common.hpp"

#include <unordered_map>
#include <unordered_set>


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

            // Update new max_slots based on new spilled slots.
            // TODO*: Will do a 'hack' in frame-layout that just checks
            // if my slot_sym table is null, and if it is default to type/align 8.
            // later with more types, store a 'SlotDec' struct that stores that information
            // earlier per slot, and for new slots, check the instruction type,
            // and fill the table for new slots. then frame layout can use that
            // new SlotDesc as the source of truth.
            f.max_slot_id = regalloc.slot_counter;
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
    // ------------------------------------------------------------
    // Small helper: rewrite a Reg if it currently holds a vreg id in map
    // ------------------------------------------------------------
    static inline void rewrite_reg_if_vreg_in_map(
        Reg& r,
        const std::unordered_map<std::size_t, PRegId>& vreg_to_preg
    ) {
        if (alt<VRegId>(r.id)) {
            VRegId v = get<VRegId>(r.id);
            auto it = vreg_to_preg.find(v.id);
            if (it != vreg_to_preg.end()) {
                r = Reg{ it->second };
            }
        }
    }

    // ------------------------------------------------------------
    // Small helper: rewrite an Operand if it is a Reg holding a mapped vreg
    // ------------------------------------------------------------
    static inline void rewrite_operand_if_reg_vreg_in_map(
        Operand& op,
        const std::unordered_map<std::size_t, PRegId>& vreg_to_preg
    ) {
        if (alt<Reg>(op)) {
            Reg r = get<Reg>(op);
            rewrite_reg_if_vreg_in_map(r, vreg_to_preg);
            op = r; // re-store (safe even if unchanged)
        }
    }

    // ------------------------------------------------------------
    // NEW HELPER: rewrite every vreg occurrence inside one instruction
    // ------------------------------------------------------------
    void rewrite_inst_regs(
        LIRInstruct& inst,
        const std::unordered_map<std::size_t, PRegId>& vreg_to_preg
    ) {
        std::visit(overloaded{
            [&](LIRLoad& load) {
                // Only rewrite dst if it's that vreg (don’t clobber unrelated regs)
                rewrite_reg_if_vreg_in_map(load.dst, vreg_to_preg);
            },
            [&](LIRStore& store) {
                rewrite_reg_if_vreg_in_map(store.src, vreg_to_preg);
            },
            [&](LIRConst& cons) {
                rewrite_reg_if_vreg_in_map(cons.dst, vreg_to_preg);
            },
            [&](LIRRet& ret) {
                rewrite_reg_if_vreg_in_map(ret.id, vreg_to_preg);
            },
            [&](LIRBinOp& binop) {
                rewrite_reg_if_vreg_in_map(binop.dst, vreg_to_preg);
                rewrite_operand_if_reg_vreg_in_map(binop.lhs, vreg_to_preg);
                rewrite_operand_if_reg_vreg_in_map(binop.rhs, vreg_to_preg);
            },
            [&](LIRSetCC& setcc) {
                rewrite_reg_if_vreg_in_map(setcc.dst, vreg_to_preg);
                rewrite_operand_if_reg_vreg_in_map(setcc.lhs, vreg_to_preg);
                rewrite_operand_if_reg_vreg_in_map(setcc.rhs, vreg_to_preg);
            },
            [&](LIRBranch& br) {
                rewrite_reg_if_vreg_in_map(br.cmp, vreg_to_preg);
            },
            [&](LIRJump& jump) {
                // Jumps don't use vregs.
            },
        }, inst.pl);
    }


    // TODO: I CHEATED OKAY?? I CHEATED. I WROTE THIS WHOLE THING AND IT WAS
    // TERRIBLE. I SAID REWRITE MY CRAP . Just refactor it to not use unordered_thing
    // later. sigh
    struct SpillRewriteResult {
        std::vector<LIRInstruct> before;
        std::vector<LIRInstruct> after;
    };

    SpillRewriteResult spill_rewrite_before_after(
        LIRInstruct& inst,
        const std::vector<Location>& locs
    ) {
        ScratchRegs scratch_regs;

        std::unordered_map<std::size_t, PRegId> vreg_to_preg;
        std::unordered_set<std::size_t> spilled_uses;
        std::unordered_set<std::size_t> spilled_defs;
        std::unordered_set<std::size_t> all_vregs;
        std::unordered_map<std::size_t, SlotId> spilled_slot;

        for (const auto& vreg : uses(inst)) {
            all_vregs.insert(vreg.id);
            const Location& L = locs.at(vreg.id);
            if (L.kind == LOC_SLOT) spilled_uses.insert(vreg.id);
        }
        for (const auto& vreg : defs(inst)) {
            all_vregs.insert(vreg.id);
            const Location& L = locs.at(vreg.id);
            if (L.kind == LOC_SLOT) spilled_defs.insert(vreg.id);
        }

        for (std::size_t vreg_id : all_vregs) {
            const Location& L = locs.at(vreg_id);
            if (L.kind == LOC_PREG) {
                vreg_to_preg.emplace(vreg_id, PRegId{L.id});
            } else if (L.kind == LOC_SLOT) {
                PRegId scratch = scratch_regs.acquire();
                vreg_to_preg.emplace(vreg_id, scratch);
                spilled_slot.emplace(vreg_id, SlotId{L.id});
            }
        }

        SpillRewriteResult res;
        res.before.reserve(spilled_uses.size());
        res.after.reserve(spilled_defs.size());

        // BEFORE: reload spilled uses
        for (std::size_t vreg_id : spilled_uses) {
            res.before.emplace_back(LIRLoad{spilled_slot.at(vreg_id), vreg_to_preg.at(vreg_id)}, /*inst_num filled later*/ 0);
        }

        // Rewrite the instruction itself (both LOC_PREG and LOC_SLOT vregs)
        rewrite_inst_regs(inst, vreg_to_preg);

        // AFTER: store spilled defs
        for (std::size_t vreg_id : spilled_defs) {
            res.after.emplace_back(LIRStore{spilled_slot.at(vreg_id), vreg_to_preg.at(vreg_id)}, /*inst_num filled later*/ 0);
        }

        return res;
    }


    
    void add_spilled_to_insts() {
        for (auto& f : ctx.lir_funcs) {
            const std::vector<Location>& locs = ctx.locs_by_func(f.id);

            for (auto& b : ctx.lir_blocks(f)) {
                std::vector<LIRInstruct> new_insts;
                std::size_t next_inst_num = 0;

                auto emit_one = [&](LIRInstruct& inst) {
                    SpillRewriteResult r = spill_rewrite_before_after(inst, locs);

                    // emit BEFORE
                    for (auto& bi : r.before) {
                        bi.inst_num = next_inst_num;
                        new_insts.push_back(bi);
                        next_inst_num += 2;
                    }

                    // emit rewritten instruction
                    inst.inst_num = next_inst_num;
                    new_insts.push_back(inst);
                    next_inst_num += 2;

                    // emit AFTER
                    for (auto& ai : r.after) {
                        ai.inst_num = next_inst_num;
                        new_insts.push_back(ai);
                        next_inst_num += 2;
                    }
                };

                // Rewrite normal instructions
                for (const auto& orig_i : ctx.lir_insts(b)) {
                    LIRInstruct inst = orig_i;
                    emit_one(inst);
                }

                // Rewrite terminator (exists only in b.term)
                assert(b.term.has_value());
                LIRInstruct term = *b.term;

                // Emit its before/after into b.insts, but keep terminator stored in b.term
                SpillRewriteResult tr = spill_rewrite_before_after(term, locs);

                // BEFORE for terminator goes into instruction list
                for (auto& bi : tr.before) {
                    bi.inst_num = next_inst_num;
                    new_insts.push_back(bi);
                    next_inst_num += 2;
                }

                // Terminator itself stays in b.term
                term.inst_num = next_inst_num;
                next_inst_num += 2;
                b.term = term;

                // AFTER for terminator (usually empty) would be unreachable, but keep it for now
                for (auto& ai : tr.after) {
                    ai.inst_num = next_inst_num;
                    new_insts.push_back(ai);
                    next_inst_num += 2;
                }

                b.insts = std::move(new_insts);
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

    static inline void maybe_push_vreg(std::vector<VRegId>& out, const Reg& r) {
        if (alt<VRegId>(r.id)) out.push_back(get<VRegId>(r.id));
    }

    static inline void maybe_push_vreg(std::vector<VRegId>& out, const Operand& op) {
        if (alt<Reg>(op)) maybe_push_vreg(out, get<Reg>(op));
    }

    // Returns set of defined vregs in an instruction.
    std::vector<VRegId> defs(const LIRInstruct& inst) {
        std::vector<VRegId> defined;

        std::visit(overloaded{
            [&](const LIRLoad& load)  { maybe_push_vreg(defined, load.dst); },
            [&](const LIRConst& cons) { maybe_push_vreg(defined, cons.dst); },
            [&](const LIRBinOp& binop){ maybe_push_vreg(defined, binop.dst); },
            [&](const LIRSetCC& setcc){ maybe_push_vreg(defined, setcc.dst); },
            [&](auto const&) { /* none */ }
        }, inst.pl);

        return defined;
    }

    // Returns set of used vregs in an instruction.
    std::vector<VRegId> uses(const LIRInstruct& inst) {
        std::vector<VRegId> used;

        std::visit(overloaded{
            [&](const LIRBinOp& binop) {
                maybe_push_vreg(used, binop.lhs);
                maybe_push_vreg(used, binop.rhs);
            },
            [&](const LIRStore& store) {
                maybe_push_vreg(used, store.src);
            },
            [&](const LIRRet& ret) {
                maybe_push_vreg(used, ret.id);
            },
            [&](const LIRSetCC& setcc){ 
                maybe_push_vreg(used, setcc.lhs);
                maybe_push_vreg(used, setcc.rhs);
            },
            [&](const LIRBranch& br){ maybe_push_vreg(used, br.cmp); },
            [&](auto const&) { /* none */ }
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
    // TODO**: Merge intervals since my linear scan algo is lazy. Must-do for now.
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
        out << "Function " << curr_func->id.id << ":\n";
        out << "After scanning Block " << curr_block->id.id << ": " << std::endl;
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

    // Now sub out Vreg -> Preg in instructions, and
    // Add spill instructions for spills.
    regalloc.add_spilled_to_insts();
    if (debug) {
        std::cout << "\n\n------ Physical register insertion/spill insertion pass ------\n" << std::endl;
        print_lir(ctx.lir_funcs, std::cout);
    }
}