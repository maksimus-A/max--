#pragma once
#include <array>
#include <cstddef>

#define ARM_FREE_REGS 7
#define ARM_SCRATCH_REGS 2

struct VRegId { std::size_t id; };
struct PRegId { std::size_t id; };

struct ScratchRegs {
    ScratchRegs() {
        r0.id = ARM_FREE_REGS;
        r1.id = ARM_FREE_REGS+1;
    }

    PRegId r0, r1;
    uint8_t used_mask = 0; // bit0 for r0, bit1 for r1

    void reset() { used_mask = 0; }

    PRegId acquire() {
        if ((used_mask & 1) == 0) { used_mask |= 1; return r0; }
        if ((used_mask & 2) == 0) { used_mask |= 2; return r1; }
        return PRegId{SIZE_MAX}; // or assert(false)
    }
};

struct Reg {
public:
    Reg(VRegId vreg_): id(vreg_) {}
    Reg(PRegId preg_): id(preg_) {}
    std::variant<VRegId, PRegId> id;
};

inline VRegId get_vreg(Reg reg) {
    return std::get<VRegId>(reg.id);
}

inline PRegId get_preg(Reg reg) {
    return std::get<PRegId>(reg.id);
}

inline std::size_t get_reg_id(Reg reg) {
    if (std::holds_alternative<VRegId>(reg.id)) {
        return get_vreg(reg).id;
    }
    else if (std::holds_alternative<PRegId>(reg.id)) {
        return get_preg(reg).id;
    }
    return SIZE_MAX;
}

