#include "ast/parser/ast.h"
#include "codegen/ir-gen/mir.h"
#include <stdbool.h>
#include <stddef.h>

#define FP_ALIGN 16 // for ARM64

typedef struct SlotFrameInfo {
    size_t size;
    size_t align;
    int32_t fp_offset;
} SlotFrameInfo;

// Frame is per-function.
typedef struct FrameInfo {
    // todo: make this 3-4 regions of memory per function instead.
    // Layout specified in arm64-mac-notes.txt
    bool uses_fp; // If we use the FP as our reference to slots. default true for now.
    bool save_fp_lr; // If we want to save FP/LR. default true for now.
    SlotFrameInfo* slot_map; // Indexed by slot_id.

    size_t locals_size; // size of all 'locals' aka slots rn
    size_t frame_record_size; // FP/LP size 
    size_t callee_save_size; // if we need to save callee args
    size_t total_frame_size; // sum of all other regions

    FuncId id; // ID of function this frame is for.
} FrameInfo;

// Size/alignment information of each 'type' (built-in for now).
typedef struct SizeAlign {
    size_t size;
    size_t align;
} SizeAlign;

typedef struct FrameLayout {
    IRModule* ir_mod;
    Diagnostics* ir_diags;
    Arena* arena;

    // I'll 'calloc' using funcs->count.
    FrameInfo* frames; // Stores each frame info created per function.

} FrameLayout;

// Table storing builtin-type->sizealign info
extern const SizeAlign builtin_type_sizealign[TYPE_TOTAL_COUNT];

void run_frame_layout(FrameLayout* frame_lay);
void frame_layout_init(FrameLayout* frame_lay, Diagnostics* ir_diags, Arena* arena, IRModule* ir_mod);
void free_frames(FrameLayout* frame_lay);

void print_frames(FrameLayout* frame_lay);