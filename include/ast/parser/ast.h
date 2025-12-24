#pragma once
#include "arena/arena.h"
#include "ast/lexer/lexer.h"
#include <stddef.h>

typedef struct ASTNode ASTNode;
typedef struct Symbol Symbol;

/*------ AST HELPERS ------*/
typedef enum ASTKind {
    AST_PROGRAM,
    AST_VAR_DEC,
    AST_INT_LIT,
    AST_NAME, // identifier expression
    AST_EXPR, // remove?
    AST_BLOCK,
    AST_EXIT,
    AST_ERROR
} ASTKind;

typedef enum BuiltInType {
    // Always add BuiltInType to string list in ast_printer.c
    TYPE_INT,
    TYPE_BOOL,
    TYPE_CHAR,
    TYPE_TOTAL_COUNT
    // todo: add more types as we go
} BuiltInType;

typedef struct SrcSpan {
    // Used to store variable name + location.
    size_t length;
    size_t start;
} SrcSpan;

/*------- INFORMATION STRUCTS -------*/

// NodeList (for funcs, program, blocks, etc)
typedef struct NodeList {
    ASTNode** items;
    size_t count;
    size_t capacity;
} NodeList;

// Entire program
typedef struct ProgramInfo {
    NodeList body;
} ProgramInfo;

// Primitives
typedef struct IntLitInfo{
    SrcSpan string_value;
    long value;
} IntLitInfo;

// Declarations
typedef struct VarDeclInfo {
    SrcSpan name_span;
    enum BuiltInType type;
    ASTNode* init_expr; // = expr;
    Symbol* symbol;
} VarDeclInfo;

// Variable name
typedef struct VarNameInfo{
    SrcSpan name_span; // var name
    Symbol* resolved_sym;
} VarNameInfo;

// Blocks
typedef struct BlockInfo {
    NodeList body;
} BlockInfo;

//Built-ins
// Exit
typedef struct ExitInfo {
    SrcSpan func_span;
    ASTNode* expr;
} ExitInfo;

// Expressions?

/*------- AST STRUCTS -------*/
typedef struct ASTNode {

    ASTKind ast_kind;
    SrcSpan span; // location info of entire 'node'

    union {
        ProgramInfo program;
        IntLitInfo int_lit;
        VarDeclInfo var_decl;
        VarNameInfo var_name;
        BlockInfo block_info;
        ExitInfo exit_info;
    } node_info;
} ASTNode;


