#include "codegen/backend/lir/arm64/lir.h"
#include "codegen/backend/frame-layout/arm64/frame_lay.h"
#include "codegen/backend/ir_walkers.h"
#include "codegen/ir-gen/mir.h"
#include "common.h"
#include <stdalign.h>

// Grabs n-th function in function list.
LIRFunction* get_nth_func_lir(Vector* lir_funcs, int i) {
    if (i == SIZE_MAX) return NULL;
    return VEC_AT_PTR_T(lir_funcs, LIRFunction, i);
}

// Grabs pointer to current IRFunction being used inside builder.
static LIRFunction* get_curr_func_lir(LIRBuilder* lir_builder, FuncId func_id) {
    if (func_id.id == SIZE_MAX) return NULL;
    return get_nth_func_lir(&lir_builder->lir_funcs, func_id.id);
}

// Grabs n-th block by index in specific function.
LIRBlock* get_nth_block_lir(LIRFunction* func, int i) {
    if (i >= func->blocks.count) return NULL;
    return VEC_AT_PTR_T(&func->blocks, LIRBlock, i);
}

static LIRBlock* get_curr_block_lir(LIRBuilder* builder, LIRFunction* func, BlockId block_id) {
    if (block_id.id == SIZE_MAX) return NULL;
    return get_nth_block_lir(func, block_id.id);
}

LIRInstruct* get_nth_instruction_lir(LIRBlock* block, int i) {
    if (i >= block->instructions.count) return NULL;
    return VEC_AT_PTR_T(&block->instructions, LIRInstruct, i);
}

// Insert LIR instruction into LIR builder.
static bool lir_insert_instruction(LIRBuilder* lir_builder, LIRInstruct lir_inst, BlockId block_id, FuncId func_id) {
    // bounds checks
    if (block_id.id == SIZE_MAX) return false;
    if (func_id.id == SIZE_MAX) return false;
    if (func_id.id >= lir_builder->lir_funcs.count) return false;
    // use macro to get pointer to current IRFunc element
    LIRFunction* f = get_curr_func_lir(lir_builder, func_id);
    if (block_id.id >= f->blocks.count) return false;

    // use macro to get pointer to current IRBlock element
    LIRBlock* b = get_curr_block_lir(lir_builder, f, block_id);
    // Add instruction to current block.
    VEC_PUSH_T(&b->instructions, lir_inst);

    return true;
}

void lir_builder_init(LIRBuilder* builder, Arena* arena, Diagnostics* ir_diags, IRModule* mod, Source* source_file) {
    // initialize LIRFunc
    VEC_INIT_T(&builder->lir_funcs, arena, LIRFunction);
    builder->next_block_id.id = 0;
    builder->curr_func_index = SIZE_MAX;
    builder->curr_block_index = SIZE_MAX;
    builder->curr_inst_index = SIZE_MAX;


    builder->ir_diags = ir_diags;
    builder->arena = arena;
    builder->source_file = source_file;
    builder->mod = mod;
}

// Creates a new LIR func.
// todo*: ensure func_id == vector index of func.
// could break later.
static void visit_func_begin(void* user, IRFunction* func) {
    LIRBuilder* builder = (LIRBuilder*)user;

    LIRFunction lir_func = {0};
    // Set curr vreg_id to 0
    lir_func.next_new_vreg = func->next_temp_id.id;
    // Create blocks vector
    VEC_INIT_T(&lir_func.blocks, builder->arena, LIRBlock);

    // Set function id
    lir_func.id = func->id;
    // lir_func.next_temp_id = (TempId) {.id=0};


    // push function into builder
    VEC_PUSH_T(&builder->lir_funcs, lir_func);

    // Set builder insertion points.
    builder->curr_func_index = func->id.id;
}

static void visit_func_end(void* user, IRFunction* func) {

}

// Creates a new LIR block.
// todo*: ensure block_id == vector index of block.
// could break later.
static void visit_block_begin(void* user, IRBlock* block) {
   LIRBuilder* builder = (LIRBuilder*)user;

    LIRBlock lir_block = {0};
    lir_block.id = block->id;

    builder->curr_block_index = block->id.id;

    VEC_INIT_T(&lir_block.instructions, builder->arena, LIRInstruct);

    LIRFunction* f = get_nth_func_lir(&builder->lir_funcs, builder->curr_func_index);

    VEC_PUSH_T(&f->blocks, lir_block);
}

static void visit_block_end(void* user, IRBlock* block) {

}

// ------ Conversion helpers ------

// Creates new vreg ID based on temp ID.
static VRegId create_vreg_id(TempId temp_id) {
    VRegId vreg_id = (VRegId) {
        .id = temp_id.id
    };

    return vreg_id;
    // todo: add to VRegInfo in IRModule. But currently temps don't store type info.
}

// Adds a new vreg without reference to a previous temp ID.
static size_t add_new_vreg(LIRFunction* func) {
    size_t new_vreg_id = func->next_new_vreg;
    func->next_new_vreg++;
    return new_vreg_id;
}

// Creates a FP/SP +- offset based on SlotId.
// todo*: hard-coded FP offset; doesn't consider SP yet.
// change to check if FP or SP.
static Mem create_mem_info(SlotId* slot_id, IRModule* mod, size_t func_index) {
    SlotFrameInfo slot_info = mod->frames[func_index].slot_map[slot_id->id];

    Mem mem = (Mem) {
        .base = LIR_BASE_FP,
        .offset = slot_info.fp_offset
    };
    return mem;
}

// Checks if value in store is immediate or temp.
static LIRValue create_lir_value(IRValue ir_val, LIRFunction* func) {
    LIRValue val = {0};
    val.value_type = ir_val.value_type;
    switch (ir_val.value_kind) {
        case IRVAL_TEMP:
        {
            VRegId vreg_id = create_vreg_id(ir_val.value_id.temp_id);
            val.value_kind = LIRVAL_VREG;
            val.value_id.vreg = vreg_id;
            break;
        }
        case IRVAL_IMM:
        {
            val.value_kind = LIRVAL_IMM;
            val.value_id.imm = ir_val.value_id.imm;
            break;
        }
    }
    return val;
}

// Create 'const' op: Materializes immediate into a register
static LIRInstruct materialize_imm(LIRBuilder* builder, int64_t imm, VRegId vreg_id) {
    LIRConst lir_const = (LIRConst) {
        .dst = vreg_id,
        .src = imm
    };

    LIRInstruct inst = (LIRInstruct) {
        .type = LIR_CONST,
        .inst_num = builder->curr_inst_index,
        .payload.const_payload = lir_const
    };
    return inst;
}



// Converts all MIR instructions into LIR instructions based on a 
// few special rules.
static void visit_instruct(void* user, IRInstruct* inst, BlockId block_id, FuncId func_id, size_t inst_index) {
    LIRBuilder* lir_builder = (LIRBuilder*)user;
    LIRFunction* f = get_curr_func_lir(lir_builder, func_id);

    switch (inst->type) {
        case IR_LOAD:
        {
            // converts temps -> vregs
            // and slot -> fp +- offset

            VRegId vreg_id = create_vreg_id(inst->payload.load_payload.dst);
            Mem mem_info = create_mem_info(&inst->payload.load_payload.src, lir_builder->mod, func_id.id);

            // Create payload
            LIRLoad load = (LIRLoad) {
                .dst = vreg_id,
                .src = mem_info
            };
            // Create instruction
            LIRInstruct lir_inst = (LIRInstruct) {
                .type = LIR_LOAD,
                .payload.load_payload = load,
                .inst_num = inst_index
            };
            if (!lir_insert_instruction(lir_builder, lir_inst, block_id, func_id)) {
                fprintf(stderr, "ERROR: Failed to insert 'Load' into LIR builder.");
                // todo*: insert into ir_diags.
            }

            break;
        }
        case IR_STORE:
        {
            // converts slot -> fp +- offset
            // and temp -> vreg
            Mem mem_info = create_mem_info(&inst->payload.store_payload.dst, lir_builder->mod, func_id.id);

            VRegId vreg_id;
            // if val is immediate, create materialization instruction
            if (inst->payload.store_payload.src.value_kind == IRVAL_IMM) {
                // Create new vreg for const instruction
                vreg_id.id = add_new_vreg(f);
                // Create const instruction
                LIRInstruct const_inst = materialize_imm(lir_builder, inst->payload.store_payload.src.value_id.imm, vreg_id);
                if (!lir_insert_instruction(lir_builder, const_inst, block_id, func_id)) {
                    fprintf(stderr, "ERROR: Failed to insert 'Const' into LIR builder.");
                    // todo*: insert into ir_diags.
                }
            }
            else if (inst->payload.store_payload.src.value_kind == IRVAL_TEMP) {
                vreg_id = create_vreg_id(inst->payload.store_payload.src.value_id.temp_id);
            }
            
            // Create payload
            LIRStore store = (LIRStore) {
                .dst = mem_info,
                .src = vreg_id
            };
            // Create instruction
            LIRInstruct lir_inst = (LIRInstruct) {
                .type = LIR_STORE,
                .payload.store_payload = store,
                .inst_num = inst_index
            };
            if (!lir_insert_instruction(lir_builder, lir_inst, block_id, func_id)) {
                fprintf(stderr, "ERROR: Failed to insert 'Store' into LIR builder.");
                // todo*: insert into ir_diags.
            }
            break;
        }
        case IR_HALT:
        {
            // converts temp -> vreg
            LIRValue code = create_lir_value(inst->payload.halt_payload.code, f);

            // Create instruction payload
            LIRHalt halt = (LIRHalt) {
                .code = code
            };
            LIRInstruct lir_inst = (LIRInstruct) {
                .type = LIR_HALT,
                .payload.halt_payload = halt,
            };
            if (!lir_insert_instruction(lir_builder, lir_inst, block_id, func_id)) {
                fprintf(stderr, "ERROR: Failed to insert 'ret' instruction into LIR Builder.");
                // todo*: insert into ir_diags.
            }
            break;
        }
        default: break;
    }

}

IRVisitor lir_gen = {
    .visit_instruct = visit_instruct,
    .visit_func_begin = visit_func_begin,
    .visit_func_end = visit_func_end,
    .visit_block_begin = visit_block_begin,
    .visit_block_end = visit_block_end
};

void run_lir_builder(LIRBuilder* lir_builder) {
    ir_walk_func_linear(&lir_gen, lir_builder, lir_builder->mod->funcs);
}

/*------ LIR PRINTING ------*/

//todo: later add 'slot(id) after inst for debugging.
// currently doesn't store that info anywhere in the instruction. :I

static void print_mem(LIRBuilder* builder, Mem mem, FILE* output) {
    if (mem.base == LIR_BASE_FP) {
        fprintf(output, "[fp%d]", mem.offset);
    }
    else if (mem.base == LIR_BASE_SP) {
        fprintf(output, "[sp%d]", mem.offset);
    }
}

static void print_instruction(LIRBuilder* builder, LIRInstruct* inst, FILE* output) {
    switch (inst->type) {
        case LIR_LOAD:
        {
            // load dst, src
            LIRLoad load = inst->payload.load_payload;
            fprintf(output, "load r%zu, ", load.dst.id);
            print_mem(builder, load.src, output);

            fprintf(output, "  ");
            break;
        }
        case LIR_STORE:
        {
            // store dst, src
            LIRStore store = inst->payload.store_payload;
            fprintf(output, "store ");
            print_mem(builder, store.dst, output);
            
            fprintf(output, ", r%zu", store.src.id);
            
            break;
        }
        case LIR_CONST:
        {
            LIRConst cons = inst->payload.const_payload;
            fprintf(output, "const r%zu, %lld", cons.dst.id, cons.src);
            break;
        }
        case LIR_HALT:
        {
            LIRHalt halt = inst->payload.halt_payload;
            if (halt.code.value_kind == LIRVAL_IMM) {
                fprintf(output, "ret %lld", halt.code.value_id.imm);
            }
            else if (halt.code.value_kind == LIRVAL_VREG) {
                fprintf(output, "ret r%zu", halt.code.value_id.vreg.id);
            }
            break;
        }

        default: break;
    }
    fprintf(output, "\n");
}

bool dump_lir(LIRBuilder* builder, FILE* output) {
    // Goes through all instructions and prints accordingly
    size_t block_index = 0;
    for (int i = 0; i < builder->lir_funcs.count; i++) {
        LIRFunction* f = get_nth_func_lir(&builder->lir_funcs, i);
        fprintf(output, "function_%zu:\n", f->id.id);

        for (int j = 0; j < f->blocks.count; j++) {
            fprintf(output, "  ");
            LIRBlock* b = get_nth_block_lir(f, j);
            fprintf(output, "block_%zu:\n", b->id.id);

            for (int k = 0; k < b->instructions.count; k++) {
                fprintf(output, "    ");
                LIRInstruct* instruction = get_nth_instruction_lir(b, k);
                print_instruction(builder, instruction, output);
            }
        }
    }
    return true;
}
