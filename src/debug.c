#include "ast/lexer/lexer.h"
#include "debug.h"


void print_all_tokens(TokenBuffer* tokens, const char *buffer) {

    for (int i = 0; i < tokens->count; i++) {
        Token token = tokens->data[i];
        printf(
            "%-10s  '%.*s'  (%d:%d)\n",
            token_kind_str[token.token_kind],
            (int)token.length,
            buffer + token.start,
            token.line,
            token.col
        );
    }
}

void pretty_print_tokens(TokenBuffer* tokens, const char *buffer) {
    for (int i = 0; i < tokens->count; i++) {
        Token token = tokens->data[i];
        printf("%s ", token_kind_str[token.token_kind]);
        fflush(stdout);
    }
    printf("\n");
}