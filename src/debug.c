#include "ast/lexer/lexer.h"

static const char* token_kind_str[TOK_COUNT+1] = {
    /* TOK_EOF */          "EOF",

    /* IF */               "IF",
    /* WHILE */            "WHILE",
    /* FN */               "FN",
    /* RETURN */           "RETURN",
    /* INT */              "INT",
    /* VOID */             "VOID",
    /* VARIABLE */         "VARIABLE",

    /* IDENTIFIER */       "IDENTIFIER",
    /* INT_LITERAL */      "INT_LITERAL",

    /* SL_COMMENT */       "SL_COMMENT",
    /* ML_COMMENT */       "ML_COMMENT",

    /* CUR_BRACK_START */  "{",
    /* CUR_BRACK_END */    "}",
    /* PAREN_START */      "(",
    /* PAREN_END */        ")",
    /* SEMICOLON */        ";",
    /* COLON */            ":",
    /* COMMA */            ",",

    /* PLUS */             "+",
    /* MINUS */            "-",
    /* MULT */             "*",
    /* DIV */              "/",
    /* EQ */               "=",
    /* NEQ */              "!=",
    /* TOKEN COUNT*/       "TOK_COUNT"
};

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