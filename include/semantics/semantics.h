#include "table/ptrtable.h"
#include <stdint.h>

typedef struct Semantics {
    uint8_t* def_assn_at;  // returns is/is not definitely assigned right before node evaluation.
    PtrTable* type_of_expr;
    PtrTable* name_resolution;
} Semantics;