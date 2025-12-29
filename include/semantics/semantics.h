#include "table/ptrtable.h"
#include <stdint.h>

typedef struct Semantics {
    PtrTable type_of_expr;
    PtrTable name_resolution; // name_resolution[symbol_id] = {string_variable};
} Semantics;