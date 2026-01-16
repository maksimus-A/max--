#include "table/ptrtable.h"
#include <stdint.h>

typedef struct Semantics {
    PtrTable type_of_expr;
    PtrTable name_resolution; // name_resolution[symbol_id] = Symbol*;
    PtrTable globals_name_map; // TODO: UNUSED
} Semantics;