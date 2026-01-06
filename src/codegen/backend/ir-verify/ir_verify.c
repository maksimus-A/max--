#include "codegen/backend/ir-verify/ir_verify.h"
#include "codegen/backend/ir_walkers.h"
#include "codegen/ir-gen/mir.h"
#include "table/ptrtable.h"
#include <stdalign.h>
#include <stdlib.h>
#include <string.h>

void visit_func_begin(void* user, IRFunction* func) {
    IRVerify* verifier = (IRVerify*)user;
    // NULL-set all values, & set size
    verifier->temp_def = calloc(func->next_temp_id.id+1, sizeof(IRInstruct*));
    verifier->temp_cap = func->next_temp_id.id+1;
}

void visit_func_end(void* user, IRFunction* func) {
    IRVerify* verifier = (IRVerify*)user;

    free(verifier->temp_def);
    verifier->temp_def = NULL;
    verifier->temp_cap = 0;
}

void visit_block_begin(void* user, IRBlock* block) {

}
void visit_block_end(void* user, IRBlock* block) {

}

void visit_instruct(void* user, IRInstruct* inst, BlockId block_id, FuncId func_id, size_t inst_index) {
    IRVerify* verifier = (IRVerify*)user;
    verifier->inst_visited++;

    switch (inst->type) {
        case IR_LOAD:
        {
            // Slot referenced exists.
            Load load = inst->payload.load_payload;
            Symbol* slot_sym = get_ptr_tbl(verifier->ir_module->name_resolution, load.src.id);
            if (slot_sym == NULL) {
                fprintf(stderr, "ERROR: Symbol %zu does not exist, but was referenced.", load.src.id);
            }
            // Slot has a known type (this is kind of useless because
            // variables get assigned a type (i think) during frontend generation, always.)
            
            // Ensure temp wasn't previously defined.
            // todo: verify that unset vectors would actully return NULL.
            if (verifier->temp_def[load.dst.id] == NULL) {
                verifier->temp_def[load.dst.id] = inst;
            }
            else {
                // todo: show which instruction it was prev defined at.
                fprintf(stderr, "ERROR: Temp t%zu was previously defined.", load.dst.id);
            }

            // Load operation types match
            // well... right now temps don't have a type.
            break;
        }
        case IR_STORE:
        {
            // Slot referenced exists
            Store store = inst->payload.store_payload;
            Symbol* slot_sym = get_ptr_tbl(verifier->ir_module->name_resolution, store.dst.id);
            if (slot_sym == NULL) {
                fprintf(stderr, "ERROR: Symbol %zu does not exist, but was referenced.", store.dst.id);
            }

            // Slot has a known type (this is kind of useless because
            // variables get assigned a type (i think) during frontend generation, always.)
            
            // Ensure temp was defined before it's used here
            if (store.src.value_kind == IRVAL_TEMP) {
                size_t val = store.src.value_id.temp_id.id;
                if (val < verifier->temp_cap) {
                    if (verifier->temp_def[val] == NULL) {
                        // todo: show which instruction it was prev defined at.
                        // you can use the outer for-loop, it 'gets nth instr'
                        fprintf(stderr, "ERROR: Temp t%zu has not been declared.", store.src.value_id.temp_id.id);
                    }
                }
                else {
                    fprintf(stderr, "ERROR: Accessing invalid temp t%zu", val);
                }
            }
            // store operation types match
            // well... right now temps don't have a type.

            // Store operand kinds are valid (no storing to temp)
            // My prev pass literally doesn't allow this because 'dst' is stored
            // as a SlotId, which cannot be a TempId.
            break;
        }
        case IR_HALT:
        {
            break;
        }

        default: break;
    }
}

IRVisitor ir_visitor = {
    .visit_instruct = visit_instruct,
    .visit_func_begin = visit_func_begin,
    .visit_func_end = visit_func_end,
    .visit_block_begin = visit_block_begin,
    .visit_block_end = visit_block_end
};

void run_ir_verifier(IRVerify* verifier, Vector* funcs) {
    ir_walk_func_linear(&ir_visitor, verifier, funcs);
}

void ir_verifier_init(IRVerify* verifier, Diagnostics* ir_diags, Arena* arena, IRModule* ir_module) {
    verifier->arena = arena;
    verifier->ir_diags = ir_diags;
    verifier->ir_module = ir_module;
    verifier->temp_cap = 0; // just in case
    verifier->inst_visited = 0;
}