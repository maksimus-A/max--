#pragma once
#include "ast/parser/ast.h"
#include "errors/diagnostics.h"

typedef struct Symbol { 
    SrcSpan symbol_span;
    const char* name;
    struct Symbol* next; 
} Symbol;

typedef struct Scope  { 
    Symbol* symbols; 
    struct Scope* parent; 
} Scope;

typedef struct Resolver { 
    Scope* scope; 
    Diagnostics* diags;
} Resolver;

void run_resolver(ASTNode* ast_root, Resolver* resolver);
void resolver_init(Resolver* resolver, Diagnostics* diags);


/*
  void (*enter_block)(void* user, ASTNode* block);
  void (*leave_block)(void* user, ASTNode* block);
  void (*on_int_decl)(void* user, ASTNode* decl);
  void (*on_exit)(void* user, ASTNode* exit_node);
  void (*on_ident)(void* user, ASTNode* identifier);*/