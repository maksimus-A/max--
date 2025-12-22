#include "arena/arena.h"
#include "ast/lexer/lexer.h"
#include <stddef.h>

typedef enum ASTKind {
    AST_PROGRAM,
    AST_VAR_DEC,
    AST_INT_LIT,
    AST_NAME, // identifier expression
} ASTKind;

typedef enum BuiltInType {
    TYPE_INT,
    TYPE_BOOL,
    TYPE_CHAR
    // todo: add more types as we go
} BuiltInType;

typedef struct SrcSpan {
    // Used to store variable name + location.
    // TODO: Make helper that computes line/col based on start/length.
    size_t length;
    size_t start;
} SrcSpan;

typedef struct ASTNode ASTNode;
struct ASTNode {

    ASTKind ast_kind;
    SrcSpan span; // location info of entire construct

    union {
        // Entire program
        struct {
            ASTNode** items;
            size_t count;
        } program;

        // Primitives
        struct {
            long value;
        } int_lit;

        // Declarations
        struct {
            SrcSpan name_span; // var name (by offset + length in buffer)
            enum BuiltInType type;
            ASTNode* init_expr;
        } var_decl;

        // Variable name
        struct {
            SrcSpan name_span; // var name
        } var_name;

    } payload;


};

// Stores current index in parsing,
typedef struct Parser Parser;
struct Parser {
    TokenBuffer* tokens;
    int token_index;
    Arena* ast_arena;
    // TODO: Make more robust error_list struct for reporting compiler errors nicely
    char** error_list;
};

ASTNode* build_ast(Parser* parser);
int initialize_parser(Parser* parser, Arena* arena, TokenBuffer* tokens);