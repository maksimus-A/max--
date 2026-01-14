#pragma once
//#include "codegen/backend/lir/arm64/lir.h"
#include "codegen/ir-gen/mir.h"

typedef struct IRVisitor {
    void (*visit_instruct)(void* user, IRInstruct* inst, IRFunction* f, IRBlock* b, size_t inst_index);
    // Function begin/end hooks
    void (*visit_func_begin)(void* user, IRFunction* func);
    void (*visit_func_end)(void* user, IRFunction* func);
    // Block begin/end hooks
    void (*visit_block_begin)(void* user, IRBlock* block, IRFunction* f);
    void (*visit_block_end)(void* user, IRBlock* block, IRFunction* f);
} IRVisitor;

/*
typedef struct LIRVisitor {
    void (*visit_instruct)(void* user, LIRInstruct* inst, BlockId block_id, FuncId func_id, size_t inst_index);
    // Function begin/end hooks
    void (*visit_func_begin)(void* user, LIRFunction* func);
    void (*visit_func_end)(void* user, LIRFunction* func);
    // Block begin/end hooks
    void (*visit_block_begin)(void* user, LIRBlock* block);
    void (*visit_block_end)(void* user, LIRBlock* block);
} LIRVisitor;*/

void ir_walk_func_linear(IRVisitor* visitor, void* user, Vector* funcs);
//void lir_walk_func_linear(LIRVisitor* visitor, void* user, Vector* funcs);