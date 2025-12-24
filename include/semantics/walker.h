#include "semantics/scope.h"

typedef struct Visitor {
    void (*pre)(void* user, ASTNode* node);
    void (*post)(void* user, ASTNode* node);
} Visitor;

void walk_node(Visitor* visitor, void* user, ASTNode* node);