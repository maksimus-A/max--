// mir = max-- intermediate representation
#include "errors/diagnostics.h"
#include "semantics/scope.h"
#include <stdint.h>

typedef enum IRInstructType {
    IR_STORE,
    IR_LOAD,
    IR_HALT // terminates program with status code.
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

typedef struct Exit { 
    IRValue code; 
} Exit;
/* ------ Instruction payloads ------*/

/* ------ IR structs ------*/
typedef struct IRInstruct {
    IRInstructType type;

    union inst_payload {
        Store store_payload;
        Load load_payload;
        Exit exit_payload;
    } payload;

    size_t ast_id;
    SrcSpan span;

} IRInstruct;

typedef struct IRBlock {
    // list of instructions
    IRInstruct* instructions;

    // number of instructions
    size_t count;
    size_t capacity;
} IRBlock;

typedef struct IRBuilder {
    // list of IRBlocks, each block has instructions
    IRBlock* blocks;

    // pointer to current block
    size_t curr_block;
    
    size_t block_count;
    size_t block_capacity;

    // diagnostics struct?
    Diagnostics* diags;
} IRBuilder;
/* ------ IR structs ------*/

