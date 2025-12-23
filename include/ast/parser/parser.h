#include "ast/parser/ast.h"
#include "ast/lexer/lexer.h"
#include "arena/arena.h"

#define DEFAULT_ERROR_LIST_SIZE 10000

// Stores current index in parsing,
typedef struct Parser Parser;
struct Parser {
    TokenBuffer* tokens;
    int token_index;
    Arena* ast_arena;
    char* error_list[DEFAULT_ERROR_LIST_SIZE];
    size_t error_list_size;
};

ASTNode* build_ast(Parser* parser, Source* source_file);
int initialize_parser(Parser* parser, Arena* arena, TokenBuffer* tokens);
int free_ast_arena(Parser* parser);

// Error helpers
void add_err_msg(Parser* parser, char* err_msg);
int print_parser_err_msgs(Parser* parser);
