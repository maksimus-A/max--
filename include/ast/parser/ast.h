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
    AST_ASSN,
    AST_BLOCK,
    AST_EXIT,
    AST_BIN_OP, // binary operators
    AST_CMP_OP,
    AST_LOG_OP,
    AST_UNARY_OP,
    // Control flow stuff
    AST_COND,
    AST_IF,
    AST_WHILE,
    // Function stuff
    AST_FN_DEC,
    AST_FN_CALL,
    AST_ERROR
} ASTKind;

typedef enum BuiltInType {
    // Always add BuiltInType to string list in ast_printer.c
    // AND add to frame_lay.c
    // TODO: Convert internally to SI32. (REMOVE)
    TYPE_INT, // converts to SI64.
    TYPE_SI64,

    TYPE_BOOL,
    TYPE_CHAR,
    TYPE_VOID,
    TYPE_UNKNOWN,
    TYPE_TOTAL_COUNT
    // todo: add more types as we go
} BuiltInType;

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
    SrcSpan name_span; // var/function name??
    Symbol* resolved_sym;
} VarNameInfo;

// Assignment statement
typedef struct AssnStmtInfo {
    SrcSpan name_span;
    enum BuiltInType type;
    Symbol* resolved_sym;
    ASTNode* init_expr;
} AssnStmtInfo;

// Blocks
typedef struct BlockInfo {
    NodeList body;
} BlockInfo;

// Binary operators
typedef struct BinaryOperatorInfo {
    Token op;
    ASTNode* LHS;
    ASTNode* RHS;
}BinOpInfo;

// Unary op
typedef struct UnaryOperatorInfo {
    Token op;
    ASTNode* expr;
} UnOpInfo;

typedef struct IfStmtInfo {
    ASTNode* cond;
    ASTNode* then_block;
    ASTNode* else_block;
} IfStmtInfo;

typedef struct WhileStmtInfo {
    ASTNode* cond;
    ASTNode* loop_block;
} WhileStmtInfo;

// Function stuff

// Not an 'AST' type, purely internal to FnDecl.
typedef struct ParamDeclInfo {
    SrcSpan name;
    BuiltInType type;
} ParamDeclInfo;

typedef struct FnDeclInfo {
    SrcSpan fn_name;
    BuiltInType ret_type;
    Vector params; // Vector of ParamDeclInfos.
    ASTNode* fn_block;
    Symbol* sym; // Assigned during scope resolution.
} FnDeclInfo;

// Used for function calls. 
typedef struct CallExprInfo {
    ASTNode* callee;         // AST_NAME/VarName
    Vector args;             // ASTNode* exprs
    // Set only after scope_res.
    Symbol* callee_sym;   // resolved function symbol
    BuiltInType ret_type;        // return type (or Unknown until resolved)
} CallExprInfo;

// Exit (turns into return now.)
typedef struct ExitInfo {
    SrcSpan func_span;
    ASTNode* expr; // NULL == no_value
} ExitInfo;


/*------- AST STRUCTS -------*/
typedef struct ASTNode {

    ASTKind ast_kind;
    SrcSpan span; // location info of entire 'node'
    size_t id;

    union {
        ProgramInfo program;
        IntLitInfo int_lit;
        VarDeclInfo var_decl;
        VarNameInfo var_name;
        BlockInfo block_info;
        ExitInfo exit_info; // return info really.
        AssnStmtInfo assn_stmt;
        BinOpInfo bin_op; // also for cmpop
        UnOpInfo un_op;
        IfStmtInfo if_stmt;
        WhileStmtInfo while_stmt;
        FnDeclInfo fn_dec;
        CallExprInfo fn_call;
    } node_info;
} ASTNode;


