// mir = max-- intermediate representation
#pragma once
#include "ast/parser/ast.h"
#include "semantics/scope.h"
#include "table/ptrtable.h"
#include "codegen/ir-gen/ir_types.h"
#include <stdint.h>


// TODO: UNUSED
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
    BuiltInType type;
} TempId;

typedef struct BlockId {
    size_t id;
} BlockId;

typedef enum IRValueKind {
    IRVAL_TEMP,
    IRVAL_IMM,
    IRVAL_ERR
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

// Operations
typedef enum {
    BIN_ADD,
    BIN_SUB,
    BIN_MUL,
    BIN_SDIV,
    BIN_UDIV,
    BIN_ERROR
} BinOpKind;

typedef struct BinOp {
    TempId dst;
    IRValue lhs;
    IRValue rhs;
    BinOpKind kind;
} BinOp;

// Comparisons
typedef enum {
    CMP_LT,
    CMP_GT,
    CMP_EQ,
    CMP_NEQ,
    CMP_ERR
    // later: CMP_LE, CMP_GT, CMP_GE
} CmpKind;

typedef struct Compare {
    TempId dst;
    IRValue lhs;
    IRValue rhs;
    CmpKind kind;
} Cmp;

// Branches to 0 if cmp == 0; branches to non-zero if cmp != 0.
typedef struct Branch {
    IRValue cmp;
    BlockId non_zero;
    BlockId zero;
} Branch;

typedef struct Jump {
    BlockId jump_to;
} Jump;
/* ------ Instruction payloads ------*/

// Each "vector" will be cast to the proper type
/* ------ IR structs ------*/
typedef struct IRInstruct {
    IRInstructType type;

    union inst_payload {
        Store store_payload;
        Load load_payload;


        // Operations
        BinOp binop_pl;
        Cmp cmp_pl; // todo: unused

        // Terminator payloads
        Halt halt_payload;
        Branch br_pl;
        Jump jump_pl;
    } payload;

    size_t ast_id;
    SrcSpan span;

} IRInstruct;



typedef struct IRBlock {
    // list of instructions
    // Vector<IRInstruct>
    Vector instructions;
    BlockId id;

    IRInstruct term; // TODO: Refactor LIR gen to see this/use this.

    Vector preds; // BlockId's
    Vector succs; // BlockId's
} IRBlock;

typedef struct FuncId {
    size_t id;
} FuncId;

typedef struct IRFunction {
    // func has list of blocks
    // Vector<IRBlock>
    Vector blocks;

    // TODO: Separate per-func slot id from global slot id.
    size_t next_slot_id;
    // Guarantee: slot id's are dense, and slot_index == slot_id.
    PtrTable slot_sym; // maps slot_id -> Symbol.

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

    // diagnostics struct? (UNUSED)?
    Diagnostics* diags;

    // Semantics tables
    Semantics* sema;

    // Source file (mostly for printing MIR)
    Source* source_file;

    // IRValue Stack
    Vector val_stack;
} IRBuilder;
/* ------ IR structs ------*/

void builder_init(IRBuilder* builder, Arena* arena, Diagnostics* diags, Semantics* sema, Source* source_file);
void run_mir_gen(ASTNode* ast_root, IRBuilder* builder);
bool dump_mir(IRBuilder* builder, FILE* output);

IRFunction* get_nth_func(Vector* funcs, int i);
IRBlock* get_nth_block(IRFunction* func, int i);
IRInstruct* get_nth_instruction(IRBlock* block, int i);


