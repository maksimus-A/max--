// mir = max-- intermediate representation
#include "errors/diagnostics.h"
#include "semantics/scope.h"
#include "vector/vec.h"
#include <stdint.h>

#define VEC_INIT_T(vec, arena, T) \
    init_vec((vec), (arena), sizeof(T), alignof(T))
#define VEC_PUSH_T(vec, value) \
    push_vec((vec), &(value))
// Gives the vector at position i.
#define VEC_AT_T(vec, T, i) \
    (((T*)(vec)->items)[(i)])


typedef enum IRInstructType {
    IR_STORE,
    IR_LOAD,
    IR_HALT // terminates program with status code. (currently exit(0))
    // no instructions allowed after exit.
} IRInstructType;

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
} IRBlock;

typedef struct IRFunction {
    // func has list of blocks
    // Vector<IRBlock>
    Vector blocks;

    // starting block in function
    BlockId entry;

    // temp register id's
    size_t next_temp_id;
    // slot table/local count?
} IRFunction;

typedef struct FuncId {
    size_t id;
} FuncId;

typedef struct IRBuilder {
    // list of IRFunctions, each function has blocks
    // Vector<IrFunction>
    Vector functions;

    // pointer to current func
    FuncId curr_func;

    // diagnostics struct?
    Diagnostics* diags;
} IRBuilder;
/* ------ IR structs ------*/

