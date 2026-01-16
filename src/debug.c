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
        if (token.token_kind == SEMICOLON) printf("\n");
        if (token.token_kind == CUR_BRACK_START) printf("\n");
        if (token.token_kind == CUR_BRACK_END) printf("\n");
        fflush(stdout);
    }
    printf("\n");
}

void print_token_kind(enum TokenKind kind) {
    printf("current token kind: %s\n", token_kind_str[kind]);
}