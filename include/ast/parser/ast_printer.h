#include <stdio.h>
#include <stdlib.h>
#include "ast/parser/ast.h"
#include "common.h"


void dump_ast(ASTNode* node, Source* source_file, int indent);