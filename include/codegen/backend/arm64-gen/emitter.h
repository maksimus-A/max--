// just text emission from LIR.
#pragma once
#include "common.h"
#include "errors/diagnostics.h"
#include "vector/vec.h"
#include <stdio.h>

typedef enum PRegs {
    x9,
    x10,
    x11,
    x12,
    x13,
    x14,
    x15,
    PREGS_SIZE
} PRegs;

typedef struct ARMEmitter {
    FILE* assm;
    Vector assm_; // todo: might not use this yet.

    Vector* funcs; // LIR funcs
    IRModule* mod;
    Diagnostics* ir_diags;
} ARMEmitter;

void arm_emitter_init(ARMEmitter* emitter, FILE* assm, Vector* lir_funcs, IRModule* mod, Diagnostics* ir_diags);
void run_arm_emitter(ARMEmitter* emitter);

