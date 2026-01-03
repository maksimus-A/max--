// mir = max-- intermediate representation
#pragma once
#include "ast/parser/ast.h"
#include "codegen/ir-gen/mir.h"
#include "errors/diagnostics.h"
#include "semantics/scope.h"
#include "table/ptrtable.h"
#include <stdint.h>

// todo: add terminators to each block.
// they can be 'return', 'br', 'brc', etc

typedef enum LIRInstructType {
    LIR_STORE,
    LIR_LOAD,
    LIR_HALT // TODO: remove later for a terminator somehow.
} LIRInstructType;

typedef enum LIRTerminator {
    LIR_TERM_RETURN,
    LIR_TERM_UBR, // unconditional branch
    LIR_TERM_CBR, // conditional branch
    LIR_TERM_RET // returns from function with 'int'.
} LIRTerminator;

/* ------ Instruction payload helpers ------*/
// todo: remove/change these to FP offsets instead?

// slots -> FP +- offset
typedef enum Base {
    LIR_BASE_FP,
    LIR_BASE_SP
} Base;

// stores FP/SP +- offset
typedef struct Mem {
    Base base;
    int32_t offset;
} Mem;

// Temp -> virtual registers
typedef struct VRegId {
    size_t id;
}VRegId;

// todo: make table in IRMod that stores vregid->VRegInfo
typedef struct VRegInfo { // virtual register (for temps)
    int32_t size;
    BuiltInType type; // optional?
}VRegInfo;

typedef enum LIRValueKind {
    LIRVAL_VREG,
    LIRVAL_IMM
} LIRValueKind;

// Stores either an Immediate (literal) or a vreg.
typedef struct LIRValue {
    LIRValueKind value_kind;
    BuiltInType value_type;
    // Represents whether val is a vreg or immediate.
    union {
        int64_t imm; // immediate
        VRegId vreg; // virtual register
    } value_id; 
} LIRValue;
/* ------ Instruction payload helpers ------*/

/* ------ Instruction payloads ------*/
typedef struct LIRStore { 
    Mem dst;
    LIRValue src;
} LIRStore;

typedef struct LIRLoad { 
    VRegId dst;
    Mem src;
} LIRLoad;

// TODO: Refactor to 'return' once functions exist.
typedef struct LIRHalt { // comes from exit(0), placeholder for 'return;'
    LIRValue code; 
} LIRHalt;
/* ------ Instruction payloads ------*/

// Each "vector" will be cast to the proper type
/* ------ IR structs ------*/
typedef struct LIRInstruct {
    LIRInstructType type;

    union lir_inst_payload {
        LIRStore store_payload;
        LIRLoad load_payload;
        LIRHalt halt_payload;
    } payload;

    size_t inst_num;

} LIRInstruct;

typedef struct LIRBlock {
    // list of instructions
    // Vector<LIRInstruct>
    Vector lir_inst;
    BlockId id;
} LIRBlock;

typedef struct LIRFunction {
    // func has list of blocks
    // Vector<LIRBlock>
    Vector blocks;

    // todo: shouldn't need this; comes from MIR.
    //size_t next_slot_id;

    // starting block in function
    BlockId entry;

    // vreg register id's
    // todo: unnecessary? they should come from existing IR.
    //VRegId next_temp_id;
    // slot table/local count?

    // Funciton ID
    FuncId id;
} LIRFunction;

typedef struct LIRBuilder {
    // list of LIRFunctions, each function has blocks
    // Vector<LIrFunction>
    Vector lir_funcs;

    // pointer to current func (the ID of the function)
    // used to index directly into vector when inserting instructions
    size_t curr_func_index;
    size_t curr_block_index;

    // Counters for creating unique IDs
    BlockId next_block_id;

    // Arena alloc
    Arena* arena;

    // diagnostics struct?
    Diagnostics* ir_diags;

    // Source file (mostly for printing LIR)
    Source* source_file;

    // IR module (useful tables)
    IRModule* mod;

    // FrameInfo (slotid -> FrameInfo table)
} LIRBuilder;
/* ------ LIR structs ------*/

void lir_builder_init(LIRBuilder* builder, Arena* arena, Diagnostics* ir_diags, IRModule* mod, Source* source_file);
void run_lir_builder(LIRBuilder* lir_builder);
bool dump_lir(LIRBuilder* builder, FILE* output);

LIRFunction* get_nth_func_lir(Vector* LIRfuncs, int i);
LIRBlock* get_nth_block_lir(LIRFunction* LIRfunc, int i);
LIRInstruct* get_nth_instruction_lir(LIRBlock* LIRblock, int i);


