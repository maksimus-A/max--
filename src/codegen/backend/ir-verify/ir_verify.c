#include "codegen/backend/ir-verify/ir_verify.h"
#include "codegen/backend/ir_walkers.h"
#include "codegen/ir-gen/ir_types.h"
#include "codegen/ir-gen/mir.h"
#include "errors/diagnostics.h"
#include "table/ptrtable.h"
#include "vector/vec.h"
#include <stdalign.h>
#include <stdlib.h>
#include <string.h>

static void verify_value_defined(IRVerify* v, IRValue val, size_t inst_index) {
    if (val.value_kind == IRVAL_TEMP) {
        size_t tid = val.value_id.temp_id.id;
        if (tid >= v->temp_cap || v->temp_def[tid] == NULL) {
            add_diag_mir(v->ir_diags, ERROR, "(verify val) At instruction %zu: Temp t%zu has not been declared.", inst_index, tid);
        }
    }
}

static void define_temp(IRVerify* verifier, TempId temp, IRInstruct* inst, size_t inst_index) {
    if (verifier->temp_def[temp.id] == NULL) {
        verifier->temp_def[temp.id] = inst;
    }
    else {
        // todo: show which instruction it was prev defined at.
        add_diag_mir(verifier->ir_diags, ERROR, "At instruction # %zu: Temp t%zu was previously defined.", inst_index, temp.id);
    }
}

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

void visit_block_begin(void* user, IRBlock* block, IRFunction* f) {

}

void visit_block_end(void* user, IRBlock* block, IRFunction* f) {
    IRVerify* verifier = (IRVerify*)user;
    if (block->term.type == IR_UNDEFINED)
        add_diag_mir(verifier->ir_diags, ERROR, 
            "Terminator in function %zu, block %zu is undefined.", f->id.id, block->id.id);

    // Check successor counts match properly.
    switch (block->term.type) {
        case IR_HALT:
        {
            if (block->succs.count != 0)
                add_diag_mir(verifier->ir_diags, ERROR, 
                    "Block %zu is terminated with HALT but has %zu successors (should be %zu)", block->id.id, block->succs.count, 0);
            break;
        }
        case IR_JUMP:
        {
            if (block->succs.count != 1)
                add_diag_mir(verifier->ir_diags, ERROR, 
                    "Block %zu is terminated with JUMP but has %zu successors (should be %zu)", block->id.id, block->succs.count, 1);
            break;
        }
        case IR_BRANCH_IF_ZERO:
        {
            if (block->succs.count != 2)
                add_diag_mir(verifier->ir_diags, ERROR, 
                    "Block %zu is terminated with BRANCH but has %zu successors (should be %zu)", block->id.id, block->succs.count, 2);
            break;
        }
        default: add_diag_mir(verifier->ir_diags, ERROR, "Terminator instruction is not actually a terminator.");
    }

    // Check block numbers are not greater than max block capacity per function.
    size_t max_block_id = f->blocks.count;
    for (int i = 0; i < block->succs.count; i++) {
        const BlockId succ = VEC_AT_T(&block->succs, BlockId, i);
        if (succ.id > max_block_id) {
            add_diag_mir(verifier->ir_diags, ERROR, "Block ID (%zu) is greater than number of blocks in function %zu.", block->id.id, f->id.id);
        }
    }
    
}

void visit_instruct(void* user, IRInstruct* inst, IRFunction* f, IRBlock* b, size_t inst_index) {
    IRVerify* verifier = (IRVerify*)user;
    verifier->inst_visited++;

    switch (inst->type) {
        case IR_LOAD:
        {
            // Slot referenced exists.
            Load load = inst->payload.load_payload;
            Symbol* slot_sym = get_ptr_tbl(verifier->ir_module->name_resolution, load.src.id);
            if (slot_sym == NULL) {
                add_diag_mir(verifier->ir_diags, ERROR, "Symbol %zu does not exist, but was referenced.", load.src.id);
            }
            // Slot has a known type (this is kind of useless because
            // variables get assigned a type (i think) during frontend generation, always.)
            
            // Ensure temp wasn't previously defined.
            // todo: verify that unset vectors would actully return NULL.
            define_temp(verifier, load.dst, inst, inst_index);

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
                add_diag_mir(verifier->ir_diags, ERROR,"Symbol %zu does not exist, but was referenced.", store.dst.id);
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
                        add_diag_mir(verifier->ir_diags, ERROR,"(Store) At instruction %zu: Temp t%zu has not been declared.", inst_index, store.src.value_id.temp_id.id);
                    }
                }
                else {
                    add_diag_mir(verifier->ir_diags, ERROR,"At instruction %zu: Accessing invalid temp t%zu", inst_index, val);
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
            const Halt halt = inst->payload.halt_payload;
            // Ensure temp was defined before it's used here
            if (halt.code.value_kind == IRVAL_TEMP) {
                size_t val = halt.code.value_id.temp_id.id;
                if (val < verifier->temp_cap) {
                    if (verifier->temp_def[val] == NULL) {
                        // todo: show which instruction it was prev defined at.
                        // you can use the outer for-loop, it 'gets nth instr'
                        add_diag_mir(verifier->ir_diags, ERROR,"At instruction %zu: Temp t%zu has not been declared.", inst_index, halt.code.value_id.temp_id.id);
                    }
                }
                else {
                    add_diag_mir(verifier->ir_diags, ERROR,"At instruction %zu: Accessing invalid temp t%zu", inst_index, val);
                }
            }
            break;
        }
        case IR_BINOP:
        {
            BinOp binop = inst->payload.binop_pl;

            define_temp(verifier, binop.dst, inst, inst_index);

            if (binop.lhs.value_kind == IRVAL_TEMP) {
                verify_value_defined(verifier, binop.lhs, inst_index);
            }
            if (binop.rhs.value_kind == IRVAL_TEMP) {
                verify_value_defined(verifier, binop.rhs, inst_index);
            }
            break;
        }
        case IR_CMPOP:
        {
            Cmp cmp = inst->payload.cmp_pl;

            define_temp(verifier, cmp.dst, inst, inst_index);

            if (cmp.lhs.value_kind == IRVAL_TEMP) {
                verify_value_defined(verifier, cmp.lhs, inst_index);
            }
            if (cmp.rhs.value_kind == IRVAL_TEMP) {
                verify_value_defined(verifier, cmp.rhs, inst_index);
            }
            break;
        }
        case IR_JUMP:
        {
            Jump jump = inst->payload.jump_pl;
            size_t max_block_id = f->blocks.count;
            if (jump.jump_to.id > max_block_id) {
                add_diag_mir(verifier->ir_diags, ERROR, "Jump instruction references block (%zu), which is greater than number of blocks in function %zu.", jump.jump_to.id, f->id.id);
            }
        }
        case IR_BRANCH_IF_ZERO:
        {
            Branch br = inst->payload.br_pl;
            size_t max_block_id = f->blocks.count;
            if (br.non_zero.id > max_block_id) {
                add_diag_mir(verifier->ir_diags, ERROR, "Branch instruction references block (%zu), which is greater than number of blocks in function %zu. (%zu)", br.non_zero.id, f->id.id, max_block_id);
            }
            if (br.zero.id > max_block_id) {
                add_diag_mir(verifier->ir_diags, ERROR, "Branch instruction references block (%zu), which is greater than number of blocks in function %zu. (%zu)", br.zero.id, f->id.id, max_block_id);
            }
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