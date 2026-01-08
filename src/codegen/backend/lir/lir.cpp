#include "codegen/backend/backend_context.hpp"
#include "codegen/backend/lir/lir.hpp"
extern "C" {
    #include "vector/vec.h"
    #include "common.h"
    #include "errors/diagnostics.h"
    #include "codegen/ir-gen/mir.h"
}
#include <iostream>

class Lir {
public:
    Lir(IRModule& mod, Source& src, Diagnostics& diags, BackendContext& cxt)
        : mod(mod), src(src), diags(diags), cxt(cxt) {}

    void lower_mir_to_lir() {
        std::cout << "Lowering mir to lir !! :3" << std::endl;
    }

private:
    IRModule& mod;
    Source& src;
    Diagnostics& diags;
    BackendContext& cxt;
    bool debug;

};

