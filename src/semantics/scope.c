#include <stdbool.h>
#include <stdlib.h>

#include "semantics/scope.h"

/*
enter_block before visiting statements inside
leave_block after visiting statements inside
on_int_decl when you hit the decl node (usually before its initializer is walked, depending on your language rule)
on_ident when you hit a AST_NAME
on_exit when you hit AST_EXIT (typically before walking its expression is fine)
*/

