#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "debug.h"
#include "ast/parser/ast.h"
#include "semantics/scope.h"
#include "semantics/walker.h"

// True if given node contains a list of children.
bool ast_has_children(ASTNode* node) {
    switch (node->ast_kind) {
        case AST_PROGRAM: return true;
        case AST_BLOCK: return true;
        default: return false;
    }
}

// Gets item list from node.
NodeList* get_item_list(ASTNode* node) {
    switch (node->ast_kind) {
        case AST_PROGRAM: return &node->node_info.program.body;
        case AST_BLOCK: return &node->node_info.block_info.body;
        default: return NULL;
    }
}

// Gets child expression from nodes that contain children expression nodes.
ASTNode* get_child_expr(ASTNode* node) {
    switch (node->ast_kind) {
        case AST_VAR_DEC: return node->node_info.var_decl.init_expr;
        case AST_EXIT: return node->node_info.exit_info.expr;
        default: return NULL;
    }
}

// Grabs specific index from node list.
ASTNode* nodelist_get(NodeList* list, size_t i) {
    if (i >= list->count) return NULL;
    
    return list->items[i];
}

// True if given nodelist contains no items.
bool nodelist_is_empty(NodeList* list) {
    return list->count == 0;
}

// Returns location of pointer in buffer to start of span
const char* span_ptr(SrcSpan span, Source* source_file) {
    return &source_file->buffer[span.start];
}

// True if the strings each span points to are the same.
bool span_eq(Source* source_file, SrcSpan a, SrcSpan b) {
    if (a.length != b.length) return false;

    const char* a_ptr = span_ptr(a, source_file);
    const char* b_ptr = span_ptr(b, source_file);

    return (memcmp(a_ptr, b_ptr, a.length) == 0);
}

void span_to_cstr(Arena* arena, Source* source_file, SrcSpan span) {

}


/*------ WALKERS -------*/
// void* means current user (resolver, type_checker, etc)
void walk_node(Visitor* visitor, void* user, ASTNode* node) {
    // Idea here: The point of the walker is just to visit
    // All the children nodes, while the visitor takes care of
    // Actually implementing what happens during each node.
    // So.. I guess... Just recursively traverse the list? DFS?
    switch (node->ast_kind) {
        case AST_PROGRAM:
        {
            NodeList* program_list = get_item_list(node);
            if (!nodelist_is_empty(program_list)) {
                for (int i = 0; i < program_list->count; i++) {
                    walk_node(visitor, user, program_list->items[i]);
                }
            }
            break;
        }
        case AST_VAR_DEC:
        {
            ASTNode* expr = get_child_expr(node);
            if (expr) walk_node(visitor, user, expr);
            break;
        }
        case AST_EXIT:
        {
            ASTNode* expr = get_child_expr(node);
            if (expr) walk_node(visitor, user,expr);
            break;
        }
        case AST_BLOCK:
        {
            NodeList* block_list = get_item_list(node);
            if (!nodelist_is_empty(block_list)) {
                for (int i = 0; i < block_list->count; i++) {
                    walk_node(visitor, user, block_list->items[i]);
                }
            }
            break;
        }
        case AST_INT_LIT:
        {
            // No idea because there's no children.
            break;
        }
        case AST_NAME:
        {
            // No idea because there's no children.
            break;
        }
        default:
        {
            // should never get here ideally.
            break;
        }
    }
}
