#pragma once
#include "common.h"
#include "codegen/ir-gen/mir.h"
#include "codegen/backend/ir_walkers.h"

typedef struct IRVerify {
    // todo: initialize this struct.
    IRModule* ir_module;
    Diagnostics* ir_diags;
    Arena* arena;
    IRInstruct** temp_def; // temps previously seen (per function)
    size_t temp_cap;

    size_t inst_visited;
} IRVerify;

void run_ir_verifier(IRVerify* verifier, Vector* funcs);
void ir_verifier_init(IRVerify* verifier, Diagnostics* ir_diags, Arena* arena, IRModule* ir_module);
