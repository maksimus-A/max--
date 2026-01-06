// ir_backend_api.h
#include "common.h"
#include "errors/diagnostics.h"
#include "vector/vec.h"
#ifdef __cplusplus
extern "C" {
#endif

int run_backend_pipeline(Vector* funcs, IRModule* mod, Arena* a, Diagnostics* diags, Source* source_file, bool debug);

#ifdef __cplusplus
}
#endif
