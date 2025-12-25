#pragma once
#include "ast/parser/ast.h"
#include "ast/lexer/lexer.h"
#include "arena/arena.h"
#include "errors/diagnostics.h"

#define DEFAULT_ERROR_LIST_SIZE 10000

// Stores current index in parsing,
typedef struct Parser Parser;
struct Parser {
    TokenBuffer* tokens;
    int token_index;
    Arena* ast_arena;
    Diagnostics* diags;
    // TODO: Remove once Diag refactor
    // is complete.
    char* error_list[DEFAULT_ERROR_LIST_SIZE];
    size_t error_list_size;
};

ASTNode* build_ast(Parser* parser, Source* source_file);
int initialize_parser(Parser* parser, Arena* arena, TokenBuffer* tokens, Diagnostics* diags);
int free_ast_arena(Parser* parser);

// Error helpers
void add_err_msg(Parser* parser, char* err_msg, size_t line, size_t col);
int print_parser_err_msgs(Parser* parser);
