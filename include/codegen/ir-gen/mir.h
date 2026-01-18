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

typedef size_t ArgId;

// Kind of like a load but only for function arguments.
typedef struct Arg {
    TempId dst;
    ArgId arg_id;
} Arg;

typedef struct Store { 
    SlotId dst;
    IRValue src;
} Store;

typedef struct Load { 
    TempId dst;
    SlotId src;
} Load;

// Exists for all functions.
// Really means 'ret' not halt.
// Bad naming from previous iterations.
// TODO: Rename halt to 'ret'. Halt is just bad naming.
typedef struct Halt {
    bool has_value;
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

typedef struct Call {
    TempId dst;
    size_t fn_sym_id;
    Vector args; // vec<IRValue>
} Call;
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
        Cmp cmp_pl;

        // Terminator payloads
        Halt halt_payload;
        Branch br_pl;
        Jump jump_pl;

        // Function arg payload
        Arg arg_pl;
        Call call_pl; 
    } payload;

    size_t ast_id;
    SrcSpan span;

} IRInstruct;



typedef struct IRBlock {
    // list of instructions
    // Vector<IRInstruct>
    Vector instructions;
    BlockId id;

    bool has_term;
    IRInstruct term;

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
    ArgId next_arg_id;
    // Guarantee: slot id's are dense, and slot_index == slot_id.
    PtrTable slot_sym; // maps slot_id -> Symbol.
    PtrTable sym_slot; // maps symbolID -> slotID (NEW)

    // starting block in function
    BlockId entry;

    // temp register id's
    TempId next_temp_id;
    // slot table/local count?

    // Funciton ID
    FuncId id;
    // Function Symbol ID.
    size_t fn_sym_id;
    // Function return type.
    BuiltInType ret_type;
    size_t param_count;
    BuiltInType* param_types;

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


