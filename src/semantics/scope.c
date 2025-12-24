#include <stdalign.h>
#include <stdbool.h>
#include <stdlib.h>

#include "semantics/scope.h"
#include "semantics/walker.h"

// TODO: Implement
void resolver_pre(void* user, ASTNode* node);
void resolver_post(void* user, ASTNode* node);

Visitor resolver_visitor = {
    .pre = resolver_pre,
    .post = resolver_post
};

void run_resolver(ASTNode* ast_root, Resolver* resolver) {
    walk_node(&resolver_visitor, resolver, ast_root);
}

void resolver_init(Resolver* resolver, Diagnostics* diags) {
    resolver->diags = diags;
    resolver->scope = NULL;
}