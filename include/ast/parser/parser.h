#include "ast/parser/ast.h"
#include "ast/lexer/lexer.h"
#include "arena/arena.h"

// Stores current index in parsing,
typedef struct Parser Parser;
struct Parser {
    TokenBuffer* tokens;
    int token_index;
    Arena* ast_arena;
    // TODO: Make more robust error_list struct for reporting compiler errors nicely
    char** error_list;
};

ASTNode* build_ast(Parser* parser, Source* source_file);
int initialize_parser(Parser* parser, Arena* arena, TokenBuffer* tokens);