#include "codegen/backend/backend_context.hpp"
#include <iostream>
#include <vector>
#include <memory>
#include <fstream>
extern "C" {
    #include "codegen/backend/backend_api.h"
    #include "codegen/ir-gen/mir.h"
    #include "ast/parser/ast.h"
    #include "errors/diagnostics.h"
}
#include "codegen/backend/ir_walkers.h"
#include "codegen/backend/lir/lir.hpp"
#include "codegen/backend/lir/lir_print.hpp"
#include "codegen/backend/visitors/mir_visitor.hpp"
#include "codegen/backend/visitors/lir_visitor.hpp"
#include "common.hpp"

// Essentially "main" for the cpp backend.
class BackendDriver {
public:
    BackendDriver(IRModule* mod, Source* src, Diagnostics* diags, bool debug, BackendContext ctx_, std::ostream& armout_)
        : mod(mod), src(src), diags(diags), debug(debug), ctx(ctx_), armout(armout_) {}

    void run(Vector* mir_funcs) {
        lower_mir_to_lir(ctx, debug);

        // does nothing yet since only terminator is 'ret'.
        construct_cfg(ctx, debug);

        // Does use/def pass,
        // IN/OUT pass.
        liveness_analysis(ctx, debug);

        // Finds live ranges of vregs, and does full regalloc,
        // including substituting vreg -> preg and spill inst insertion.
        regalloc(ctx, debug);

        // Constructs frame layouts per function
        run_frame_layout(ctx, debug);

        // Emits ARM into specified file.
        emit_arch_arm_64(ctx, debug, armout);
    }

private:
    IRModule* mod;
    Source* src;
    Diagnostics* diags;
    BackendContext ctx;
    bool debug;
    std::ostream& armout;

};

extern "C" int run_backend_pipeline(Vector* funcs, IRModule* mod, Arena* a, Diagnostics* diags, Source* source_file, bool debug) {
    try {
        std::ofstream armout("mxout_new.s");
        BackendContext ctx(*mod);
        BackendDriver driver(mod, source_file, diags, debug, ctx, armout);
        driver.run(funcs);
        return 0; // Success
    } 
    catch (const std::exception& e) {
        std::cerr << "ICE (Internal Compiler Error) in Backend: " << e.what() << "\n";
        return 1; // Error
    }
    catch (...) {
        std::cerr << "Unknown Error in Backend.\n";
        return 2; // Error
    }
}