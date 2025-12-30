#pragma once
#include "codegen/ir-gen/mir.h"

typedef struct IRVisitor {
    void (*visit_instruct)(void* user, IRInstruct* instruct, BlockId block_id, FuncId func_id, size_t inst_index);
    // Function begin/end hooks
    void (*visit_func_begin)(void* user, IRFunction* func);
    void (*visit_func_end)(void* user, IRFunction* func);
    // Block begin/end hooks
    void (*visit_block_begin)(void* user, IRBlock* block);
    void (*visit_block_end)(void* user, IRBlock* block);
} IRVisitor;