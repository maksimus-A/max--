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

// todo: consider moving to 'walker.h'
typedef struct Visitor {
    void (*pre)(void* user, ASTNode* node);
    void (*post)(void* user, ASTNode* node);
} Visitor;

/*
  void (*enter_block)(void* user, ASTNode* block);
  void (*leave_block)(void* user, ASTNode* block);
  void (*on_int_decl)(void* user, ASTNode* decl);
  void (*on_exit)(void* user, ASTNode* exit_node);
  void (*on_ident)(void* user, ASTNode* identifier);*/