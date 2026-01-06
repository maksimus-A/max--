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

// Essentially "main" for the cpp backend.
class BackendDriver {
public:
    BackendDriver(IRModule* mod, Source* src, Diagnostics* diags, bool debug)
        : mod(mod), src(src), diags(diags), debug(debug) {}

    void run(Vector* mir_funcs) {
        lower_mir_to_lir(mir_funcs);
    }

private:
    IRModule* mod;
    Source* src;
    Diagnostics* diags;
    bool debug;

    void lower_mir_to_lir(Vector* mir_funcs) {
        std::cout << "Running da driver lower to lir !! :3";
    }

};

extern "C" int run_backend_pipeline(Vector* funcs, IRModule* mod, Arena* a, Diagnostics* diags, Source* source_file, bool debug) {
    try {
        BackendDriver driver(mod, source_file, diags, debug);
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