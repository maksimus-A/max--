// mir = max-- intermediate representation
#include "errors/diagnostics.h"
#include "semantics/scope.h"
#include <stdint.h>

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
    IRInstruct* instructions;

    // number of instructions
    size_t count;
    size_t capacity;
} IRBlock;

typedef struct IRFunction {
    // func has list of blocks
    IRBlock* blocks;

    size_t count;
    size_t capacity;
    BlockId entry;

    // temp register id's
    size_t next_temp_id;
    // slot table/local count?
} IRFunction;

typedef struct IRBuilder {
    // list of IRFunctions, each function has blocks
    IRFunction* functions;

    // pointer to current func
    size_t curr_func;

    size_t count;
    size_t capacity;

    // diagnostics struct?
    Diagnostics* diags;
} IRBuilder;
/* ------ IR structs ------*/

