#pragma once
#include <array>
#include <cstddef>

struct VRegId { std::size_t id; };
struct PRegId { std::size_t id; };

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

