#pragma once
#include "arena/arena.h"
#include "ast/lexer/lexer.h"
#include <stddef.h>

typedef struct ASTNode ASTNode;

/*------ AST HELPERS ------*/
typedef enum ASTKind {
    AST_PROGRAM,
    AST_VAR_DEC,
    AST_INT_LIT,
    AST_NAME, // identifier expression
    AST_EXPR
} ASTKind;

typedef enum BuiltInType {
    TYPE_INT,
    TYPE_BOOL,
    TYPE_CHAR,
    TYPE_TOTAL_COUNT
    // todo: add more types as we go
} BuiltInType;

typedef struct SrcSpan {
    // Used to store variable name + location.
    // TODO: Make helper that computes line/col based on start/length.
    size_t length;
    size_t start;
} SrcSpan;

/*------- INFORMATION STRUCTS -------*/
// Entire program
typedef struct ProgramInfo {
    ASTNode** items;
    size_t count;
    size_t capacity;
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
} VarDeclInfo;

// Variable name
typedef struct VarNameInfo{
    SrcSpan name_span; // var name
} VarNameInfo;

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
    } node_info;
} ASTNode;


