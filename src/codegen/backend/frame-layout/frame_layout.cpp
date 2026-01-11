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

            // Get # of slots
            // TODO**: this doesn't account for spill slots.
            // and spill slots don't have a symbol or type.
            // I need to construct some sort of type table for them.
            size_t slots_used = f.max_slot_id;
            // Define insertion cursor
            size_t cursor = 0;
            std::vector<SlotFrameInfo> slot_map;
            for (size_t i = 0; i < slots_used; i++) {
                out << "i: " << i << std::endl;
                // I have made per-function slot id's, stored in the function.
                // The maximum slot id == next_slot_id.
                const Symbol* sym = ctx.slot_syms[f.id.id][i];
                if (sym == NULL) {
                    fprintf(stdout, "ERROR: Symbol not found in slot_id table in frame_layout.");
                    return;
                }
                out << "Symbol id: " << sym->id << std::endl;
                BuiltInType sym_type = sym->type;
                out << "Symbol type: " << sym_type << std::endl;

                // Get alignment/size of symbol (based on symbol type)
                SizeAlign slot_sa = builtin_type_sizealign[sym_type];
                // Set cursor alignment
                cursor = align_up(cursor, slot_sa.align);
                // Calculate FP's offset of slot
                int fp_offset = -(cursor + slot_sa.size);

                // Assign values into slot_map
                // Make local slot map, and assign it to frameinfo slot map.
                SlotFrameInfo slot_frame_info = SlotFrameInfo(slot_sa.size, slot_sa.align, fp_offset);

                // Assign slot map
                slot_map.push_back(slot_frame_info);

                cursor += slot_sa.size;
            }
            size_t locals_size= align_up(cursor, FP_ALIGN);
            size_t frame_record_size = align_up(16, FP_ALIGN);
            size_t callee_save_size = 0;
            size_t total_frame_size = align_up(locals_size + frame_record_size + callee_save_size, FP_ALIGN);
            // Assign found values into FrameInfo obj
            FrameInfo frame = FrameInfo(true, true, locals_size, frame_record_size,
                                        callee_save_size, total_frame_size, std::move(slot_map));
            
            ctx.frame_info[f.id.id] = std::move(frame);
        }

    }


    void print_slot_frame_info(const std::vector<SlotFrameInfo>& slot_map) {
        int i = 0;
        for (const auto& sfi: slot_map) {
            out << "    Slot " << i;
            out << " size=" << sfi.size << " align=" << sfi.align << " fp_offset=" << sfi.fp_offset << std::endl;
            i++;
        }
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

};

void run_frame_layout(BackendContext& ctx, bool debug) {
    FrameLayout frame_lay(ctx, debug, std::cout);
    frame_lay.define_frame_layout_of_slots();

    if (debug) frame_lay.print_frames();
}




