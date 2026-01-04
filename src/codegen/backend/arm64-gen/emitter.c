#include "codegen/backend/arm64-gen/emitter.h"
#include "codegen/backend/ir_walkers.h"
#include "codegen/backend/lir/arm64/lir.h"
#include "codegen/ir-gen/mir.h"
#include "codegen/backend/frame-layout/arm64/frame_lay.h"
#include "common.h"
#include "errors/diagnostics.h"
#include <stdint.h>

// Temporary map between vregs -> pregs
const char* vir_to_preg[PREGS_SIZE] = {
    "x9",
    "x10",
    "x11",
    "x12",
    "x13",
    "x14",
    "x15"
};

const char* FP = "x29"; // frame pointer
const char* SP = "sp"; // stack pointer
const char* ret_reg = "x0"; // return register
const char* LR = "x30"; // link register
const char* epilogue_name = ".Lreturn_func"; // todo: always add func_id
const char* _main = "_main";
const char* NO_RA = "ERROR: RA not implemented; more than 7 virtual registers allocated.";

static bool check_vregs_size(size_t vreg_id) {
    if (vreg_id >= PREGS_SIZE) {
        // todo: add to ir diags.
        fprintf(stderr, "%s (%zu made).\n", NO_RA, vreg_id);
        return false;
    }
    return true;
}

// Printer helpers

// Prints register as pointer in text.
void print_reg_as_addr(ARMEmitter* emitter, Mem mem) {
    if (mem.base == LIR_BASE_FP) 
        fprintf(emitter->assm, "[%s, #%d]", FP, mem.offset);
    else if (mem.base == LIR_BASE_SP)
        fprintf(emitter->assm, "[%s, #%d]", SP, mem.offset);
}

/*------ Instruction Emitters ------*/

// Emits 'load register' operation based on LIR inst.
static void emit_ldr_inst(ARMEmitter* emitter, LIRInstruct* inst) {
    fprintf(emitter->assm, "\tldr ");
    size_t vreg_id = inst->payload.load_payload.dst.id;
    if (!check_vregs_size(vreg_id)) return;

    // Print destination register
    const char* preg = vir_to_preg[vreg_id];
    fprintf(emitter->assm, "%s, ", preg);

    // Print source (from FP)
    print_reg_as_addr(emitter, inst->payload.load_payload.src);
}

// Emits 'store register' operation based on LIR inst.
static void emit_str_inst(ARMEmitter* emitter, LIRInstruct* inst) {
    fprintf(emitter->assm, "\tstr ");
    size_t vreg_id = inst->payload.store_payload.src.id;
    if (!check_vregs_size(vreg_id)) return;

    // Print source register
    const char* preg = vir_to_preg[vreg_id];
    fprintf(emitter->assm, "%s, ", preg);

    // Print destination (from FP)
    print_reg_as_addr(emitter, inst->payload.store_payload.dst);
}

// Emits moving value to register based on LIR inst.
static void emit_mov_inst(ARMEmitter* emitter, LIRInstruct* inst) {
    fprintf(emitter->assm, "\tmov ");
    size_t vreg_id = inst->payload.const_payload.dst.id;
    if (!check_vregs_size(vreg_id)) return;

    // Print destination register
    const char* preg = vir_to_preg[vreg_id];
    fprintf(emitter->assm, "%s, ", preg);

    // Print source (always immediate for now)
    fprintf(emitter->assm, "#%llu", inst->payload.const_payload.src);
}

// Only emits 'mov' return value to x0. Epilogue emits final 'ret'.
static void emit_ret_begin_inst(ARMEmitter* emitter, LIRInstruct* inst, FuncId func_id) {
    LIRHalt halt = inst->payload.halt_payload;
    const char* dst = ret_reg;

    // Move imm/vreg into x0 (return register)
    if (halt.code.value_kind == LIRVAL_IMM) {
        int64_t imm = halt.code.value_id.imm;
        fprintf(emitter->assm, "\tmov %s, #%llu\n", dst, imm);
    }
    else if (inst->payload.halt_payload.code.value_kind == LIRVAL_VREG) {
        size_t vreg_id = halt.code.value_id.vreg.id;
        if (!check_vregs_size(vreg_id)) return;

        const char* preg = vir_to_preg[vreg_id];
        fprintf(emitter->assm, "\tmov %s, %s\n", dst, preg);
    }

    // Emit branch instruction (to epilogue)
    fprintf(emitter->assm, "\tb %s%zu", epilogue_name, func_id.id);
}

// OK. There should be 'emit add', 'emit sub', but
// no instructions are associated to those yet.
// and they never would be here, b/c they're prologue/epilogues.
// I could make a simpler interface that just takes the args directly.


static void emit_default_prologue(ARMEmitter* emitter, FuncId func_id) {
    /*stp x29, x30, [sp, #-16]!
    mov x29, sp
    */
   
    // Get total frame size and allocate using sp
    size_t f_id = func_id.id;
    size_t frame_size = emitter->mod->frames[f_id].total_frame_size;
    size_t frame_record_size = emitter->mod->frames[f_id].frame_record_size;

    fprintf(emitter->assm, "\tstp %s, %s, [%s, #-%zu]!\n", FP, LR, SP, frame_record_size);
    fprintf(emitter->assm, "\tmov %s, %s\n", FP, SP);


    fprintf(emitter->assm, "\tsub %s, %s, #%zu\n", SP, SP, frame_size);
}

static void emit_default_epilogue(ARMEmitter* emitter, FuncId func_id) {

    // Get total frame size and de-allocate using sp
    size_t f_id = func_id.id;
    size_t frame_size = emitter->mod->frames[f_id].total_frame_size;
    size_t frame_record_size = emitter->mod->frames[f_id].frame_record_size;

    fprintf(emitter->assm, "\tadd %s, %s, #%zu\n", SP, SP, frame_size);
    fprintf(emitter->assm, "\tldp %s, %s, [%s], #%zu\n", FP, LR, SP, frame_record_size);
    // Return from function.
    fprintf(emitter->assm, "\tret\n");
}

static void emit_directives(ARMEmitter* emitter) {
    // todo*: this should put all functions inside global (or whatever is labelled as global)
    // and not be hard_coded 'main'.
    fprintf(emitter->assm, "\t.text\n");
    fprintf(emitter->assm, "\t.p2align 2\n");
    fprintf(emitter->assm, "\t.globl %s\n", _main);
}

/*------ Visitor hooks ------*/
static void visit_func_begin(void* user, LIRFunction* func) {
    ARMEmitter* emitter = (ARMEmitter*)user;
    // TODO: DON'T HARDCODE 'MAIN' LABEL.
    fprintf(emitter->assm, "%s:\n", _main);

    emit_default_prologue(emitter, func->id);
}

static void visit_func_end(void* user, LIRFunction* func) {
    ARMEmitter* emitter = (ARMEmitter*)user;

    fprintf(emitter->assm, "%s%zu:\n", epilogue_name, func->id.id);
    emit_default_epilogue(emitter, func->id);
}


static void visit_block_begin(void* user, LIRBlock* block) {
    ARMEmitter* emitter = (ARMEmitter*)user;
    if (block->id.id != 0) {
        // todo: emit label if not entry block.
    }
}

static void visit_block_end(void* user, LIRBlock* block) {

}

static void visit_instruct(void* user, LIRInstruct* inst, BlockId block_id, FuncId func_id, size_t inst_index) {

    ARMEmitter* emitter = (ARMEmitter*)user;

    switch (inst->type) {
        case LIR_LOAD:
        {
            emit_ldr_inst(emitter, inst);
            break;
        }
        case LIR_STORE:
        {
            emit_str_inst(emitter, inst);
            break;
        }
        case LIR_CONST:
        {
            emit_mov_inst(emitter, inst);
            break;
        }
        case LIR_HALT:
        {
            emit_ret_begin_inst(emitter, inst, func_id);
            break;
        }
        default: break;
    }
    fprintf(emitter->assm, "\n");

}

LIRVisitor lir_visitor = {
    .visit_instruct = visit_instruct,
    .visit_func_begin = visit_func_begin,
    .visit_func_end = visit_func_end,
    .visit_block_begin = visit_block_begin,
    .visit_block_end = visit_block_end
};

void run_arm_emitter(ARMEmitter* emitter) {
    emit_directives(emitter);

    lir_walk_func_linear(&lir_visitor, emitter, emitter->funcs);
}

void arm_emitter_init(ARMEmitter* emitter, FILE* assm, Vector* lir_funcs, IRModule* mod, Diagnostics* ir_diags) {
    emitter->assm = assm;
    emitter->funcs = lir_funcs;
    emitter->mod = mod;
    emitter->ir_diags = ir_diags;
}