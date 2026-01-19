#include <vector>
extern "C" {
    #include "codegen/ir-gen/mir.h"
}
#include <cstdint>

#define FP_ALIGN 16 // for ARM64
#define REG_SIZE 8

struct SlotFrameInfo {
    explicit SlotFrameInfo(size_t size_, size_t align_, int32_t fp_offset_)
        : size(size_), align(align_), fp_offset(fp_offset_) {}
    size_t size;
    size_t align;
    int32_t fp_offset;
};

// Frame is per-function.
struct FrameInfo {

    explicit FrameInfo(bool uses_fp_, bool save_fp_lr_,
                    size_t locals_size_, size_t frame_record_size_,
                    size_t callee_save_size_, size_t total_frame_size_,
                    std::vector<SlotFrameInfo> slot_map_,
                    std::vector<int32_t> callee_reg_offsets_)
    : uses_fp(uses_fp_),
        save_fp_lr(save_fp_lr_),
        locals_size(locals_size_),
        frame_record_size(frame_record_size_),
        callee_save_size(callee_save_size_),
        total_frame_size(total_frame_size_),
        slot_map(std::move(slot_map_)),
        callee_reg_offsets(std::move(callee_reg_offsets_))
        {}

    // todo: make this 3-4 regions of memory per function instead.
    // Layout specified in arm64-mac-notes.txt
    bool uses_fp; // If we use the FP as our reference to slots. default true for now.
    bool save_fp_lr; // If we want to save FP/LR. default true for now.
    std::vector<SlotFrameInfo> slot_map; // Indexed by slot_id.
    std::vector<int32_t> callee_reg_offsets; // UNUSED. indexed by RegId.

    size_t locals_size; // size of all 'locals' aka slots rn
    size_t frame_record_size; // FP/LP size 
    size_t callee_save_size; // if we need to save callee args
    size_t total_frame_size; // sum of all other regions

    FuncId id; // ID of function this frame is for.
};

// Size/alignment information of each 'type' (built-in for now).
struct SizeAlign {
    size_t size;
    size_t align;
};

// typedef struct FrameLayout {
//     IRModule* ir_mod;
//     Diagnostics* ir_diags;
//     Arena* arena;

//     // I'll 'calloc' using funcs->count.
//     FrameInfo* frames; // Stores each frame info created per function.

// } FrameLayout;