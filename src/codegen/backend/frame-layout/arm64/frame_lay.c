#include "codegen/backend/frame-layout/arm64/frame_lay.h"
#include "codegen/backend/ir_walkers.h"
#include "codegen/ir-gen/mir.h"
#include <stdalign.h>
#include <stdlib.h>

const SizeAlign builtin_type_sizealign[TYPE_TOTAL_COUNT] = {
    // todo: remove type_int internally.
    [TYPE_INT]  = { .size = 8, .align = 8 },
    [TYPE_SI64] = { .size = 8, .align = 8 },
    [TYPE_BOOL] = { .size = 1, .align = 1 },
    [TYPE_CHAR] = { .size = 1, .align = 1 },
};

static void frame_info_init(Arena* arena, FrameInfo* frame_info, size_t n_slots, FuncId id, bool uses_fp, bool save_fp_lr) {
    frame_info->uses_fp = uses_fp;
    frame_info->save_fp_lr = save_fp_lr;
    frame_info->id = id;

    frame_info->locals_size = 0;
    frame_info->frame_record_size = save_fp_lr ? 16 : 0;
    frame_info->callee_save_size = 0; // todo: modify later once this is implemented (funcs).
    frame_info->total_frame_size = frame_info->frame_record_size;

    frame_info->slot_map = (SlotFrameInfo*)arena_alloc(arena, sizeof(SlotFrameInfo)*n_slots, alignof(SlotFrameInfo));
}

static int align_up(size_t cursor, size_t align) {
    int padding = cursor % align;
    if (padding == 0) return cursor;

    return cursor + (align - padding);
}

// Calculates FP offsets of locals and total frame size.
// todo: this should take into account FP/LR locations (and eventually callees).
static void visit_func_begin(void* user, IRFunction* func) {

    FrameLayout* frame_lay = (FrameLayout*)user;

    // Init FrameInfo object
    frame_info_init(frame_lay->arena, &frame_lay->frames[func->id.id], func->next_slot_id,
        func->id, true, true);

    // Make local slot map, and assign it to frameinfo slot map.
    SlotFrameInfo slot_frame_info = {0};

    // Get # of slots
    size_t slots_used = func->next_slot_id;
    // Define insertion cursor
    size_t cursor = 0;
    for (size_t i = 0; i < slots_used; i++) {
        // I have made per-function slot id's, stored in the function.
        // The maximum slot id == next_slot_id.
        Symbol* sym = get_ptr_tbl(&func->slot_sym, i);
        if (sym == NULL) {
            fprintf(stdout, "ERROR: Symbol not found in slot_id table in frame_layout.");
            return;
        }
        BuiltInType sym_type = sym->type;

        // Get alignment/size of symbol (based on symbol type)
        SizeAlign slot_sa = builtin_type_sizealign[sym_type];
        // Set cursor alignment
        cursor = align_up(cursor, slot_sa.align);
        // Calculate FP's offset of slot
        int fp_offset = -(cursor + slot_sa.size);

        // Assign values into slot_map
        slot_frame_info.align = slot_sa.align;
        slot_frame_info.size = slot_sa.size;
        slot_frame_info.fp_offset = fp_offset;

        // Assign slot map
        // todo: do i have to memcpy??
        frame_lay->frames[func->id.id].slot_map[i] = slot_frame_info;

        cursor += slot_sa.size;
    }
    size_t locals_size= align_up(cursor, FP_ALIGN);
    // Assign found values into FrameInfo obj
    FrameInfo* frame = &frame_lay->frames[func->id.id];
    frame->locals_size = locals_size;
    
    frame->total_frame_size = align_up(frame->locals_size + frame->frame_record_size 
        + frame->callee_save_size, FP_ALIGN);

}

static void visit_func_end(void* user, IRFunction* func) {
    FrameLayout* frame_lay = (FrameLayout*)user;

}

static void visit_block_begin(void* user, IRBlock* block) {

}
static void visit_block_end(void* user, IRBlock* block) {

}

static void visit_instruct(void* user, IRInstruct* inst, BlockId block_id, FuncId func_id, size_t inst_index) {

}

IRVisitor frame_visitor = {
    .visit_instruct = visit_instruct,
    .visit_func_begin = visit_func_begin,
    .visit_func_end = visit_func_end,
    .visit_block_begin = visit_block_begin,
    .visit_block_end = visit_block_end
};

void run_frame_layout(FrameLayout* frame_lay) {
    ir_walk_func_linear(&frame_visitor, frame_lay, frame_lay->ir_mod->funcs);
}

// FREE AFTER THIS IS CALLED!
void frame_layout_init(FrameLayout* frame_lay, Diagnostics* ir_diags, Arena* arena, IRModule* ir_mod) {
    frame_lay->arena = arena;
    frame_lay->ir_diags = ir_diags;
    frame_lay->ir_mod = ir_mod;
    
    frame_lay->frames = arena_alloc(frame_lay->arena, sizeof(FrameInfo)*ir_mod->funcs->count, alignof(FrameInfo));
}

void print_slot_frame_info(SlotFrameInfo* slot_map, size_t slot_number) {
    for (size_t i = 0; i < slot_number; i++) {
        SlotFrameInfo sfi = slot_map[i];
        fprintf(stdout, "\t  Slot %zu: ", i);
        fprintf(stdout, "size=%zu align=%zu fp_offset=%d\n", sfi.size, sfi.align, sfi.fp_offset);
    }
}

void print_frames(FrameLayout* frame_lay) {
    for (size_t i = 0; i < frame_lay->ir_mod->funcs->count; i++) {
        FrameInfo frame = frame_lay->frames[i];

        IRFunction *f = get_nth_func(frame_lay->ir_mod->funcs, i);
        size_t slot_num = f->next_slot_id;

        SlotFrameInfo* slot_map = frame.slot_map;

        fprintf(stdout, "Frame %zu:\n", i);
        fprintf(stdout, "\tlocals size: %zu\n\tframe record size: %zu\n\tcallee save size: %zu\n\ttotal frame size: %zu",
            frame.locals_size, 
            frame.frame_record_size, 
            frame.callee_save_size, 
            frame.total_frame_size);
        fprintf(stdout, "\n\tSlot information:\n");
        print_slot_frame_info(slot_map, slot_num);
    }

}


