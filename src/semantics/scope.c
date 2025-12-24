#include <stdalign.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "arena/arena.h"
#include "ast/parser/ast.h"
#include "common.h"
#include "semantics/scope.h"
#include "semantics/walker.h"


bool symbols_eq(SrcSpan a, SrcSpan b, Source* source_file) {
    if (a.length != b.length) return false;

    const char* a_start_ptr = &source_file->buffer[a.start];
    const char* b_start_ptr = &source_file->buffer[b.start];

    if (memcmp(a_start_ptr, b_start_ptr, a.length) == 0) return true;

    return false;
}

// True if symbol already exists in scope
bool symbol_in_scope(Scope* scope, SrcSpan span, Resolver* resolver) {
    Symbol* symbol = scope->symbols;

    while (symbol != NULL) {
        if (symbols_eq(symbol->symbol_span, span, resolver->source_file)) return true;
        symbol = symbol->next;
    }
    return false;
}

// user = Resolver
void resolver_pre(void* user, ASTNode* node) {
    Resolver* resolver = (Resolver*)user;
    switch (node->ast_kind) {
        case AST_PROGRAM:
        {
            // Push scope
            Scope* scope = (Scope*)arena_alloc(resolver->arena, sizeof(Scope), alignof(Scope));
            scope->parent = NULL;
            scope->symbols = NULL;
            resolver->scope = scope;
            break;
        }
        case AST_BLOCK:
        {
            // Push scope
            Scope* new_scope = (Scope*)arena_alloc(resolver->arena, sizeof(Scope), alignof(Scope));
            new_scope->parent = resolver->scope;
            new_scope->symbols = NULL;
            resolver->scope = new_scope;
            break;
        }
        case AST_VAR_DEC: 
        {
            // todo: declare symbol after ensuring it's not inside symbol table
            // Declare name in current scope
            Symbol* symbol = (Symbol*)arena_alloc(resolver->arena, sizeof(Symbol), alignof(Symbol));
            symbol->symbol_span = node->node_info.var_decl.name_span;
            // Check if name already exists in scope
            if (symbol_in_scope(resolver->scope, symbol->symbol_span, resolver)) {
                char* err_msg = alloc_error(resolver->diags);
                const char* ptr = &resolver->source_file->buffer[symbol->symbol_span.start];

                snprintf(err_msg, DEFAULT_ERR_MSG_SIZE,
                        "Symbol '%.*s' has been previously declared.",
                        (int)symbol->symbol_span.length, ptr);
                LineCol line_col = get_line_col_from_span(symbol->symbol_span.start, resolver->source_file);
                add_diag(resolver->diags, ERROR, symbol->symbol_span, err_msg, line_col.line, line_col.col);
                break;
            }
            // Push symbol into scope (insert at head)
            symbol->next = resolver->scope->symbols;
            resolver->scope->symbols = symbol;
            break;
        }
        case AST_NAME:
        {
            // Check for name up scope chain
            Scope* scope = resolver->scope;
            SrcSpan wanted = node->node_info.var_name.name_span;
            bool found = false;
            while (scope != NULL) {
                if (symbol_in_scope(scope, wanted, resolver)) {
                    found = true;
                    break;
                }
                scope = scope->parent;
            }
            if (!found) {
                char* err_msg = alloc_error(resolver->diags);
                const char* ptr = &resolver->source_file->buffer[wanted.start];

                snprintf(err_msg, DEFAULT_ERR_MSG_SIZE,
                        "Symbol '%.*s' has not been declared.",
                        (int)wanted.length, ptr);
                LineCol line_col = get_line_col_from_span(wanted.start, resolver->source_file);
                add_diag(resolver->diags, ERROR, wanted, err_msg, line_col.line, line_col.col);
                break;
            }
            break;
        }

        default: break;
    }
}

void resolver_post(void* user, ASTNode* node) {
    Resolver* resolver = (Resolver*)user;
    switch (node->ast_kind) {
        case AST_PROGRAM:
        {
            // Pop scope
            resolver->scope = resolver->scope->parent;
            break;
        }
        case AST_BLOCK:
        {
            // Pop scope
            resolver->scope = resolver->scope->parent;
            break;
        }

        default: break;
    }
}

Visitor resolver_visitor = {
    .pre = resolver_pre,
    .post = resolver_post
};

void run_resolver(ASTNode* ast_root, Resolver* resolver) {
    walk_node(&resolver_visitor, resolver, ast_root);
}

void resolver_init(Resolver* resolver, Arena* arena, Diagnostics* diags, Source* source_file) {
    resolver->diags = diags;
    resolver->arena = arena;
    resolver->scope = NULL;
    resolver->source_file = source_file;
}