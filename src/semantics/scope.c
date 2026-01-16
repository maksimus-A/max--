#include <assert.h>
#include <stdalign.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arena/arena.h"
#include "ast/parser/ast.h"
#include "common.h"
#include "debug.h"
#include "errors/diagnostics.h"
#include "semantics/scope.h"
#include "semantics/walker.h"
#include "table/ptrtable.h"
#include "vector/vec.h"

// todo: modify to accommodate 'int x;'(solved?)

void print_fn_symbol_table(Resolver* res, FILE* out);

// Adds symbol according to symbol_id.
void add_symbol_to_table(Semantics* sema, Symbol* sym) {
    set_ptr_tbl(&sema->name_resolution, sym, sym->id);
}

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

// Returns symbol (if it exists inside scope)
Symbol* get_symbol(Scope* scope, SrcSpan span, Resolver* resolver) {
    Symbol* symbol = scope->symbols;
    while (symbol != NULL) {
        if (symbols_eq(symbol->symbol_span, span, resolver->source_file)) return symbol;
        symbol = symbol->next;
    }
    return NULL;
}

Symbol* create_var_or_param_symbol(SrcSpan sym_span, SymbolKind sym_kind, enum BuiltInType type, Resolver* resolver) {
    Symbol* symbol = (Symbol*)arena_alloc(resolver->arena, sizeof(Symbol), alignof(Symbol));
    symbol->symbol_span = sym_span;
    symbol->kind = sym_kind,
    symbol->type = type;
    symbol->id = resolver->curr_id;
    resolver->curr_id++;

    return symbol;
}

Symbol* create_fn_symbol(SrcSpan sym_span, FnSig* fn_sig, Resolver* resolver) {
    // todo: check this doesn't break ?? IDK. fn_sig might need to be added from arena too man.
    Symbol* symbol = (Symbol*)arena_alloc(resolver->arena, sizeof(Symbol), alignof(Symbol));
    symbol->symbol_span = sym_span;
    symbol->kind = SYM_FN;
    symbol->fn_sig = *fn_sig;
    symbol->id = resolver->curr_id;
    resolver->curr_id++;

    return symbol;
}

// Push scope (only once one already exists).
Scope* push_scope(Resolver* resolver) {
    Scope* new_scope = (Scope*)arena_alloc(resolver->arena, sizeof(Scope), alignof(Scope));
    new_scope->parent = resolver->scope;
    new_scope->symbols = NULL;
    resolver->scope = new_scope;

    return new_scope;
}

// First pass upon entering 'program':
// Checks for all global 'fn' definitions.
void collect_global_symbols(ASTNode* node, Resolver* res) {
    ProgramInfo program = node->node_info.program;

    for (int i = 0; i < program.body.count; i++) {
        ASTNode* item = program.body.items[i];

        if (item->ast_kind != AST_FN_DEC) continue;

        FnDeclInfo* fn_dec = &item->node_info.fn_dec;  // <-- pointer, not copy

        FnSig fn_sig = (FnSig){
            .name = fn_dec->fn_name,
            .ret_type = fn_dec->ret_type,
            .param_count = fn_dec->params.count,
        };

        if (fn_sig.param_count > 0) {
            fn_sig.param_types = arena_alloc(res->arena,
                fn_sig.param_count * sizeof(BuiltInType),
                alignof(BuiltInType));
            for (size_t j = 0; j < fn_sig.param_count; j++) {
                ParamDeclInfo param = VEC_AT_T(&fn_dec->params, ParamDeclInfo, (int)j);
                fn_sig.param_types[j] = param.type;
            }
        } else {
            fn_sig.param_types = NULL;
        }

        Symbol* fn_sym = create_fn_symbol(fn_dec->fn_name, &fn_sig, res);
        add_symbol_to_table(res->semantics, fn_sym);

        fn_dec->sym = fn_sym; // <-- bind back into AST

        // Add symbol to global scope at head
        fn_sym->next = res->scope->symbols;
        res->scope->symbols = fn_sym;
    }
}

// Hook that runs before visiting a node/its children.
// user = Resolver
WalkChildren resolver_pre(void* user, ASTNode* node) {
    // todo: add 'WARN' if we see 'int x = x;'
    Resolver* resolver = (Resolver*)user;
    switch (node->ast_kind) {
        case AST_PROGRAM:
        {
            // Push scope (create new)
            Scope* scope = (Scope*)arena_alloc(resolver->arena, sizeof(Scope), alignof(Scope));
            scope->parent = NULL;
            scope->symbols = NULL;
            resolver->scope = scope;
            if (resolver->debug) dump_scope_stack(resolver);

            // Collect all global functions (for now. Maybe module level vars later.)
            collect_global_symbols(node, resolver);
            break;
        }
        case AST_BLOCK:
        {
            // Push scope
            push_scope(resolver);

            break;
        }
        case AST_VAR_DEC: 
        {
            // Check if name already exists in scope
            if (symbol_in_scope(resolver->scope, node->node_info.var_decl.name_span, resolver)) {
                create_and_add_diag_fmt(resolver->diags, ERROR, node->node_info.var_decl.name_span, 
                    "Symbol '%.*s' has been previously declared.", resolver->source_file);
                break;
            }
            // currently only place symbol is created at all, but be careful
            // about adding them to the table properly.
            // Declare var in current scope
            Symbol* symbol = (Symbol*)arena_alloc(resolver->arena, sizeof(Symbol), alignof(Symbol));
            symbol->symbol_span = node->node_info.var_decl.name_span;
            symbol->kind = SYM_VAR;
            symbol->type = node->node_info.var_decl.type;
            symbol->id = resolver->curr_id;
            resolver->curr_id++;
            add_symbol_to_table(resolver->semantics, symbol);
            // Add symbol to node
            node->node_info.var_decl.symbol = symbol;
            // Push symbol into scope (insert at head)
            symbol->next = resolver->scope->symbols;
            resolver->scope->symbols = symbol;

            break;
        }
        case AST_ASSN:
        {
            // Check for name up scope chain
            Scope* scope = resolver->scope;
            SrcSpan wanted = node->node_info.assn_stmt.name_span;
            Symbol* name_symbol = NULL;

            while (scope != NULL) {
                name_symbol = get_symbol(scope, wanted, resolver);
                if (name_symbol != NULL) {
                    break;
                }
                scope = scope->parent;
            }

            if (name_symbol == NULL) {
                create_and_add_diag_fmt(resolver->diags, ERROR, node->node_info.assn_stmt.name_span,
                    "Symbol '%.*s' has not been declared.", resolver->source_file);
                break;
            }
            if (name_symbol->kind != SYM_VAR){
                create_and_add_diag_fmt(resolver->diags, ERROR, node->node_info.assn_stmt.name_span, 
                    "Symbol '%.*s' is not an assignable variable.", resolver->source_file);
                break;
            }
            else {
                 // Add symbol to node
                node->node_info.assn_stmt.resolved_sym = name_symbol;
            }

            break;
        }
        case AST_FN_DEC:
        {
            // This should be declared in global scope already.
            // But we need to orchestrate symbols for parameters existing.
            // First push scope for parameters.
            push_scope(resolver);

            FnDeclInfo fndec = node->node_info.fn_dec;
            for (size_t i = 0; i < fndec.params.count; i++) {
                // Declare symbol per parameter
                ParamDeclInfo param = VEC_AT_T(&fndec.params, ParamDeclInfo, i);
                Symbol* param_sym = create_var_or_param_symbol(param.name, SYM_PARAM, param.type, resolver);
                add_symbol_to_table(resolver->semantics, param_sym);

                // Push symbol into scope (insert at head)
                param_sym->next = resolver->scope->symbols;
                resolver->scope->symbols = param_sym;
            }
            return WALK_CHILDREN;
        }
        case AST_FN_CALL:
        {
            // Checking if the function that is being called exists.
            // Check for name up scope chain
            Scope* scope = resolver->scope;
            assert(node->node_info.fn_call.callee != NULL);
            SrcSpan wanted = node->node_info.fn_call.callee->node_info.var_name.name_span;
            printf("\n\nwanted span: %zu, %zu\n", wanted.start, wanted.length);
            bool found = false;
            while (scope != NULL) {
                if (symbol_in_scope(scope, wanted, resolver)) {
                    found = true;
                    break;
                }
                scope = scope->parent;
            }
            if (!found) { // adds error to diags
                create_and_add_diag_fmt(resolver->diags, ERROR, node->node_info.fn_call.callee->node_info.var_name.name_span,
                    "Symbol '%.*s' has not been declared.", resolver->source_file);
                break;
            }
            // Add symbol to CallExpr (we verified it exists)
            node->node_info.fn_call.callee_sym = get_symbol(scope, wanted, resolver);
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
            if (!found) { // adds error to diags
                create_and_add_diag_fmt(resolver->diags, ERROR, node->node_info.var_name.name_span, 
                    "Symbol '%.*s' has not been declared.", resolver->source_file);
                break;
            }
            // Add symbol to AST_NAME (we verified it exists)
            node->node_info.var_name.resolved_sym = get_symbol(scope, wanted, resolver);
            break;
        }

        default: break;
    }
    if (resolver->debug) dump_scope_stack(resolver);
    return WALK_CHILDREN;
}

// Hook that runs after visiting a node.
void resolver_post(void* user, ASTNode* node) {
    Resolver* resolver = (Resolver*)user;
    switch (node->ast_kind) {
        case AST_PROGRAM:
        {
            if (resolver->debug) dump_scope_stack(resolver);
            // Pop scope
            resolver->scope = resolver->scope->parent;
            break;
        }
        case AST_BLOCK:
        {
            if (resolver->debug) dump_scope_stack(resolver);
            // Pop scope
            resolver->scope = resolver->scope->parent;
            break;
        }
        case AST_FN_DEC:
        {

            if (resolver->debug) dump_scope_stack(resolver);
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

// Main function called to resolve scope and symbols.
void run_resolver(ASTNode* ast_root, Resolver* resolver) {
    walk_node(&resolver_visitor, resolver, ast_root);

    if (resolver->debug) print_fn_symbol_table(resolver, stdout);
}

void resolver_init(Resolver* resolver, Arena* arena, Diagnostics* diags, Source* source_file, bool debug) {
    resolver->diags = diags;
    resolver->arena = arena;
    resolver->scope = NULL;
    resolver->source_file = source_file;
    resolver->curr_id = 0;
    resolver->debug = debug;

    // TODO: IDK if the resolver should handle this stuff
    // about the semantics table since lots of passes will use it probably.
    PtrTable name_res;
    ptr_table_init(&name_res, arena);
    resolver->semantics = arena_alloc(arena, sizeof(Semantics), alignof(Semantics));
    resolver->semantics->name_resolution = name_res;
}

// Prints symbol at specified span (start, length)
void print_symbol(SrcSpan span, Source* source_file) {
    const char* ptr = &source_file->buffer[span.start];
    fprintf(stdout, "%.*s", (int)span.length, ptr);
}

// Prints current scope (and any parent scopes)
void dump_scope_stack(Resolver* res) {
    fprintf(stdout, "[scope dump] ");
    Scope* scope = res->scope;
    fprintf(stdout, "current=%p\n", scope);

    int depth = 0;
    while (scope != NULL) {
        fprintf(stdout, "\tdepth %d scope=%p parent=%p: symbols: {", depth, scope, scope->parent);
        Symbol* sym = scope->symbols;
        while (sym != NULL) {
            print_symbol(sym->symbol_span, res->source_file);
            if (sym->next != NULL) fprintf(stdout, ", ");
            sym = sym->next;
        }
        fprintf(stdout, "}\n");
        scope = scope->parent;
        depth++;
    }
    fprintf(stdout, "-----------------\n");
}

void print_fn_symbol_table(Resolver* res, FILE* out) {
    fprintf(out, "\nFUNCTION SIGNATURE TABLE:\n");
    PtrTable name_res = res->semantics->name_resolution;
    for (int i = 0; i < name_res.count; i++) {
        Symbol* sym = get_ptr_tbl(&name_res, i);
        if (sym->kind == SYM_FN) {
            FnSig fnsig = sym->fn_sig;
            fprintf(out, "Symbol ID: %zu ", sym->id);
            fprintf(out, "Function name: ");
            print_symbol(sym->symbol_span, res->source_file);
            fprintf(out, "  ret_type: %s, param types:", built_in_type_string[fnsig.ret_type]);

            for (int j = 0; j < fnsig.param_count; j++) {
                fprintf(out, " %s", built_in_type_string[fnsig.param_types[j]]);
            }
            if (fnsig.param_count == 0) fprintf(out, " N/A");
            fprintf(out, "\n");
        }
        else if (sym->kind == SYM_VAR) {
            fprintf(out, "Var Symbol ID: %zu\n", sym->id);
        }
    }
    fprintf(out, "\n\n");
}