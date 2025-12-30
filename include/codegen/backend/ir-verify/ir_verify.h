#pragma once
#include "common.h"
#include "codegen/ir-gen/mir.h"
#include "codegen/backend/ir_walkers.h"

typedef struct IRVerify {
    // todo: might not need this at all.
    IRModule* ir;
} IRVerify;


void ir_walk_func_linear(IRVisitor* visitor, void* user, Vector* funcs);
