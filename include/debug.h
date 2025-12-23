#pragma once
#include "ast/lexer/lexer.h"

static const char* token_kind_str[TOK_COUNT+1] = {
    /* TOK_EOF */          "EOF",

    /* IF */               "IF",
    /* WHILE */            "WHILE",
    /* FN */               "FN",
    /* RETURN */           "RETURN",
    /* INT */              "INT",
    /* VOID */             "VOID",

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
    /* LESS_THAN */        "<",


    /* NO TOKEN */         "NO_TOKEN",
    /* TOKEN COUNT*/       "TOK_COUNT"
};

void print_all_tokens(TokenBuffer* tokens, const char *buffer);
void pretty_print_tokens(TokenBuffer* tokens, const char *buffer);