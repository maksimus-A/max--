
// todo: i'll probably do this later.
// right now i'll make a literal text emitter.

typedef enum ARMInstruct {
    ARM_STP,        // Store pair to mem
    ARM_LDP,        // Load pair from mem
    ARM_MOV,        // Move dst, src (both regs)
    ARM_SUB,        // Subtract res a1 a2 (a1-a2)
    ARM_ADD,        // Add res a1 a2 (a1+a2)
    ARM_LDR,        // Load reg, Rd [Rn]
    ARM_STR,        // Store reg, Rs [Rn] (Rs -> addr in Rn)
    ARM_RET         // return from func (x30??)
} ARMInstruct;

typedef struct ARMLdp {

} ARMLdp;

typedef struct ARMStp {

} ARMStp;

typedef struct ARMMov {

} ARMMov;

typedef struct ARMSub {

} ARMSub;

typedef struct ARMAdd {

} ARMAdd;

typedef struct ARMLdr {

} ARMLdr;