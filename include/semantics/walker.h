#include "semantics/scope.h"

typedef enum WalkChildren {
    WALK_CHILDREN,
    SKIP_CHILDREN,
    WALK_UNSET
} WalkChildren;

typedef struct Visitor {
    WalkChildren (*pre)(void* user, ASTNode* node);
    void (*post)(void* user, ASTNode* node);
} Visitor;



void walk_node(Visitor* visitor, void* user, ASTNode* node);
ASTNode* get_child_expr(ASTNode* node);