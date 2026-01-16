#pragma once
#include "ast/parser/ast.h"
#include "errors/diagnostics.h"
#include "semantics/semantics.h"

typedef enum SymbolKind {
    SYM_VAR,
    SYM_FN,
    SYM_PARAM
}SymbolKind;

// Function signature
typedef struct FnSig {
    SrcSpan name;
    BuiltInType ret_type;
    size_t param_count;
    BuiltInType* param_types; // arena owned
} FnSig;

// Either var, param, or function dec.
typedef struct Symbol {
    SrcSpan symbol_span;
    SymbolKind kind;
    size_t id;
    struct Symbol* next;

    union {
        BuiltInType type;
        FnSig fn_sig;
    };
} Symbol;

typedef struct Scope  { 
    Symbol* symbols; 
    struct Scope* parent; 
} Scope;

typedef struct Resolver { 
    Scope* scope; 
    Diagnostics* diags;
    Arena* arena;
    Source* source_file;
    size_t curr_id; // local var id
    bool debug;
    Semantics* semantics; // CURRENTLY: for name_resolution table
} Resolver;

void run_resolver(ASTNode* ast_root, Resolver* resolver);
void resolver_init(Resolver* resolver, Arena* arena, Diagnostics* diags, Source* source_file, bool debug);

void dump_scope_stack(Resolver* res);


/*
  void (*enter_block)(void* user, ASTNode* block);
  void (*leave_block)(void* user, ASTNode* block);
  void (*on_int_decl)(void* user, ASTNode* decl);
  void (*on_exit)(void* user, ASTNode* exit_node);
  void (*on_ident)(void* user, ASTNode* identifier);*/