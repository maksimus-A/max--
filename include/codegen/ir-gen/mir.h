// mir = max-- intermediate representation
#pragma once
#include "errors/diagnostics.h"
#include "semantics/scope.h"
#include "table/ptrtable.h"
#include <stdint.h>

#define VEC_INIT_T(vec, arena, T) \
    vec_init((vec), (arena), sizeof(T), alignof(T))
#define VEC_PUSH_T(vec, value) \
    vec_push((vec), &(value))
// Gives the item in vector at position i.
#define VEC_AT_T(vec, T, i) \
    (((T*)(vec)->items)[(i)])
// Gives a pointer to item in vector at position i.
#define VEC_AT_PTR_T(vec, T, i) \
    (&((T*)(vec)->items)[(i)])

// todo: add terminators to each block.
// they can be 'return', 'br', 'brc', etc

typedef enum IRInstructType {
    IR_STORE,
    IR_LOAD,
    IR_HALT // terminates program with status code. (currently exit(0))
    // no instructions allowed after exit.
} IRInstructType;

typedef enum IRTerminator {
    IR_TERM_RETURN,
    IR_TERM_UBR, // unconditional branch
    IR_TERM_CBR // conditional branch
} IRTerminator;

/* ------ Instruction payload helpers ------*/
typedef struct SlotId {
    size_t id;
} SlotId;

typedef struct TempId {
    size_t id;
} TempId;

typedef enum IRValueKind {
    IRVAL_TEMP,
    IRVAL_IMM
} IRValueKind;

// Stores either an Immediate (literal) or a Temp.
typedef struct IRValue {
    IRValueKind value_kind;
    BuiltInType value_type;
    // Represents whether val is a temp or immediate.
    union {
        int64_t imm; // immediate
        TempId temp_id;
    } value_id; 
} IRValue;
/* ------ Instruction payload helpers ------*/

/* ------ Instruction payloads ------*/
typedef struct Store { 
    SlotId dst;
    IRValue src;
} Store;

typedef struct Load { 
    TempId dst;
    SlotId src;
} Load;

// TODO: Refactor to 'return' once functions exist.
typedef struct Halt { // comes from exit(0), placeholder for 'return;'
    IRValue code; 
} Halt;
/* ------ Instruction payloads ------*/

// Each "vector" will be cast to the proper type
/* ------ IR structs ------*/
typedef struct IRInstruct {
    IRInstructType type;

    union inst_payload {
        Store store_payload;
        Load load_payload;
        Halt halt_payload;
    } payload;

    size_t ast_id;
    SrcSpan span;

} IRInstruct;

typedef struct BlockId {
    size_t id;
} BlockId;

typedef struct IRBlock {
    // list of instructions
    // Vector<IRInstruct>
    Vector instructions;
    BlockId id;
} IRBlock;

typedef struct FuncId {
    size_t id;
} FuncId;

typedef struct IRFunction {
    // func has list of blocks
    // Vector<IRBlock>
    Vector blocks;

    // starting block in function
    BlockId entry;

    // temp register id's
    TempId next_temp_id;
    // slot table/local count?

    // Funciton ID
    FuncId id;
} IRFunction;

typedef struct IRBuilder {
    // list of IRFunctions, each function has blocks
    // Vector<IrFunction>
    Vector funcs;

    // pointer to current func (the ID of the function)
    // used to index directly into vector when inserting instructions
    size_t curr_func_index;
    size_t curr_block_index;

    // Counters for creating unique IDs
    BlockId next_block_id;

    // Arena alloc
    Arena* arena;

    // Symbol/slot table
    PtrTable slots;

    // diagnostics struct?
    Diagnostics* diags;

    // Semantics tables
    Semantics* sema;

    // Source file (mostly for printing MIR)
    Source* source_file;
} IRBuilder;
/* ------ IR structs ------*/

void builder_init(IRBuilder* builder, Arena* arena, Diagnostics* diags, Semantics* sema, Source* source_file);
void run_mir_gen(ASTNode* ast_root, IRBuilder* builder);
bool dump_mir(IRBuilder* builder, FILE* output);

IRFunction* get_nth_func(Vector* funcs, int i);
IRBlock* get_nth_block(IRFunction* func, int i);
IRInstruct* get_nth_instruction(IRBlock* block, int i);


