#include "semantics/scope.h"
#include "semantics/semantics.h"
#include "table/ptrtable.h"

// Definite Assignment Analyzer
typedef struct DefAssn { 
    Diagnostics* diags;
    Arena* arena;
    Source* source_file;
    uint64_t* assigned;  // STORED AS BITSET. Returns is/is not definitely assigned right before node evaluation.
    bool debug;
} DefAssn;

bool definite_assignment_init(DefAssn* defassn, Diagnostics* diags, Arena* arena, Source* source_file, bool debug, size_t total_locals);
void run_definite_assignment(ASTNode* ast_root, DefAssn* defassn);


// todo: garbage function? remove?
bool name_resolution_init(PtrTable* name_res, Arena* arena);