#pragma once
#include "ast/lexer/lexer.h"
#include "ast/parser/ast.h"

static const char* token_kind_str[TOK_COUNT+1] = {
    /* TOK_EOF */          "EOF",

    /* IF */               "IF",
    /* ELSE */             "ELSE",
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
    /* EQQ */              "==",
    /* NEQ */              "!=",
    /* LESS_THAN */        "<",
    /* GREATER_THAN */     ">",


    /* NO TOKEN */         "NO_TOKEN",
    /* TOKEN COUNT*/       "TOK_COUNT"
};


static char* built_in_type_string[TYPE_TOTAL_COUNT] = {
    /*TYPE_INT*/         "int", // Converts to i32

    // Signed integers
    /*TYPE_SI64*/        "i64",

    /*TYPE_BOOL*/        "bool",
    /*TYPE_CHAR*/        "char",
    /*TYPE VOID*/        "void",
    /*TYPE UNKNOWN*/     "UNKNOWN_TYPE"
};

void print_all_tokens(TokenBuffer* tokens, const char *buffer);
void pretty_print_tokens(TokenBuffer* tokens, const char *buffer);

void print_token_kind(enum TokenKind kind);