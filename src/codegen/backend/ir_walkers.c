#include "codegen/backend/ir-verify/ir_verify.h"
#include "codegen/ir-gen/mir.h"
#include "codegen/backend/ir_walkers.h"

/*------ Helpers ------*/

// Walk all functions, blocks, and instructions
// linearly (in vector order).
void ir_walk_func_linear(IRVisitor* visitor, void* user, Vector* funcs) {

    for (size_t i = 0; i < funcs->count; i++) {
        IRFunction* f = get_nth_func(funcs, i);

        visitor->visit_func_begin(user, f);
        for (size_t j = 0; j < f->blocks.count; j++) {
            IRBlock* b = get_nth_block(f, j);

            visitor->visit_block_begin(user, b);
            for (size_t k = 0; k < b->instructions.count; k++) {
                IRInstruct* instruction = get_nth_instruction(b, k);

                visitor->visit_instruct(user, instruction, b->id, f->id, k);
            }
            visitor->visit_block_end(user, b);
        }
        visitor->visit_func_end(user, f);
    }
}

// todo: eventually make an lir walker for emitting
// actual code. can use above for lir gen!
