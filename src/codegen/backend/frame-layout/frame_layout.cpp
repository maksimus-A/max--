#include "codegen/backend/backend_context.hpp"
#include "codegen/backend/reg_ids.hpp"
#include <stdalign.h>
#include <stdlib.h>

const SizeAlign builtin_type_sizealign[TYPE_TOTAL_COUNT] = {
    // todo: remove type_int internally.
    [TYPE_INT]  = { .size = 8, .align = 8 },
    [TYPE_SI64] = { .size = 8, .align = 8 },
    [TYPE_BOOL] = { .size = 1, .align = 1 },
    [TYPE_CHAR] = { .size = 1, .align = 1 },
};

struct FrameLayout {
public:

    explicit FrameLayout(BackendContext& ctx_, bool debug_, std::ostream& out_)
        :ctx(ctx_), debug(debug_), out(out_) {

        }

void define_frame_layout_of_slots() {
        ctx.frame_info.resize(ctx.lir_funcs.size());

        for (auto& f: ctx.lir_funcs) {

            // Start Depth at 16 (Reserved for FP/LR)
            //    [FP+0] = Old FP, [FP+8] = LR
            size_t current_depth = 16; 
            
            // --- CALLEE SAVED REGISTERS ---
            std::vector<int>& used_callee_regs = ctx.fn_used_callee_regs[f.id.id];
            
            // Prepare the lookup table
            std::vector<int32_t> callee_reg_offsets;
            callee_reg_offsets.assign(64, 0);

            for (int reg_id : used_callee_regs) {
                // Grow depth by 8 bytes
                current_depth += 8;
                
                callee_reg_offsets[reg_id] = -((int32_t)current_depth);
            }

            // Calculate size used purely by callee regs (for stats)
            size_t callee_save_size = used_callee_regs.size() * 8;

            // OPTIONAL: Re-align depth to 16 bytes before starting slots?
            // This keeps the "Locals" block aligned, which is good practice.
            current_depth = align_up(current_depth, 16);


            // --- SLOTS (LOCALS/SPILLS) ---
            std::vector<SlotFrameInfo> slot_map;
            size_t slots_used = f.max_slot_id;
            
            // Calculate size used purely by locals (start tracking relative to current depth)
            size_t start_of_locals = current_depth;

            for (size_t i = 0; i < slots_used; i++) {
                // I have made per-function slot id's, stored in the function.
                // The maximum slot id == next_slot_id.
                const Symbol* sym = ctx.slot_syms[f.id.id][i];
                // HACK TO FIX LATER!
                SizeAlign slot_sa;
                if (sym == NULL) {
                    if (debug) out << "Slot " << i << ": Symbol not found in table. Assuming new spilled slot, and defaulting to type=8, align=8." <<std::endl;

                    slot_sa = SizeAlign{8, 8};
                } else {
                    BuiltInType sym_type = (sym->kind == SYM_VAR) ? sym->type : TYPE_INT; // fallback
                    slot_sa = builtin_type_sizealign[sym_type];
                }

                // Align the CURRENT depth for this specific variable
                current_depth = align_up(current_depth, slot_sa.align);

                // Grow depth by size
                current_depth += slot_sa.size;

                // Offset is negative depth
                int32_t fp_offset = -((int32_t)current_depth);

                slot_map.emplace_back(slot_sa.size, slot_sa.align, fp_offset);
            }

            size_t locals_size = current_depth - start_of_locals;
            size_t frame_record_size = 16;
            
            // Total frame size must be 16-byte aligned for SP
            size_t total_frame_size = align_up(current_depth, 16);

            // Construct FrameInfo
            FrameInfo frame = FrameInfo(
                true, true, 
                locals_size, 
                frame_record_size,
                callee_save_size, 
                total_frame_size, 
                std::move(slot_map), 
                std::move(callee_reg_offsets)
            );
            
            ctx.frame_info[f.id.id] = std::move(frame);
        }
    }

    void add_prologue_epilogue() {
        
    }


    void print_frames() {
        for (size_t i = 0; i < ctx.frame_info.size(); i++) {
            const auto& opt = ctx.frame_info[i];
            if (!opt.has_value()) {
                // either skip or assert; skipping can hide bugs
                fprintf(stdout, "ERROR: missing FrameInfo for function %zu\n", i);
                continue; // or return/abort
            }
            const FrameInfo& frame = *opt;


            size_t max_slot_num = ctx.lir_funcs[i].max_slot_id;

            //SlotFrameInfo* slot_map = frame.slot_map;

            out << "Frame (function " << i << "):" << std::endl; 
            out << "  locals size: " << frame.locals_size << std::endl;
            out << "  frame record size: " << frame.frame_record_size<< std::endl;
            out << "  callee save size: " << frame.callee_save_size << std::endl;
            out << "  total frame size: " << frame.total_frame_size << std::endl;
            out << "  Slot Information: " << std::endl;
            print_slot_frame_info(frame.slot_map);
        }
        out << "Frame Layout completed!\n\n";
    }


private:
    BackendContext& ctx;
    bool debug;
    std::ostream& out;

    int align_up(size_t cursor, size_t align) {
        int padding = cursor % align;
        if (padding == 0) return cursor;

        return cursor + (align - padding);
    }

    void print_slot_frame_info(const std::vector<SlotFrameInfo>& slot_map) {
        int i = 0;
        for (const auto& sfi: slot_map) {
            out << "    Slot " << i;
            out << " size=" << sfi.size << " align=" << sfi.align << " fp_offset=" << sfi.fp_offset << std::endl;
            i++;
        }
    }



};

void run_frame_layout(BackendContext& ctx, bool debug) {
    FrameLayout frame_lay(ctx, debug, std::cout);
    frame_lay.define_frame_layout_of_slots();

    if (debug) frame_lay.print_frames();
}




