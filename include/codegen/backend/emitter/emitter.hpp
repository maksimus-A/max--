extern "C" {
    #include "codegen/ir-gen/mir.h"
}
#include <cstdint>
#include <ostream>

enum class ArchReg: uint8_t {
    X0, X1, X2, X3, X4, X5, 
    X6, X7, X8, X9, X10,
    X11, X12, X13, X14, X15,
    X16, X17, X18, X19, X20,
    X21, X22, X23, X24, X25,
    X26, X27, X28, X29, X30, 
    SP
};

struct Mem {
    const char* base;
    std::int32_t offset;
};

inline Mem mem(const char* base, std::int32_t offset) {
    return { base, offset };
}

inline std::ostream& operator<<(std::ostream& os, const Mem& m) {
    return os << "[" << m.base << ", #" << m.offset << "]";
}

// Printing function return labels.
struct FuncRetLabel {
    FuncId fid;
};

inline FuncRetLabel func_ret_label(FuncId fid) {
    return { fid };
}

inline std::ostream& operator<<(std::ostream& os, const FuncRetLabel& l) {
    return os << ".Lreturn_func" << l.fid.id;
}

