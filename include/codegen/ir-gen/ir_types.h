#pragma once
typedef enum IRInstructType {
    IR_STORE,
    IR_LOAD,
    IR_BINOP,
    IR_CMPOP, // comparison operation
    // Terminators.
    IR_JUMP, // unconditional branch/jump
    IR_BRANCH_IF_ZERO,
    // todo: move this to IRTerminator list.
    IR_HALT, // terminates program with status code. (currently return 0;)
    // no instructions allowed after exit.
    IR_UNDEFINED
} IRInstructType;
