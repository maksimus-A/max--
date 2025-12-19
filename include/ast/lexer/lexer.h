#pragma once
#include <stddef.h>
#include <stdio.h>
#include "common.h"

// Tokens will be stored by their length, and pointer to
// their place in the file being lexed.

enum TokenKind {
    IF,
    WHILE,
    PLUS,
    MINUS,
    MULT,
    DIV,
    FN,
    IDENTIFIER,
    SL_COMMENT,
    ML_COMMENT,
    RETURN,
    INT_LITERAL,
    CUR_BRACK_START,
    CUR_BRACK_END,
    PAREN_START,
    PAREN_END,
    SEMICOLON,
    COLON,
    VARIABLE,
    INT,
    VOID,
    COMMA,
    EQ,
    NEQ,
    TOK_EOF
};

typedef struct Token Token;
struct Token {
    enum TokenKind token_kind;
    size_t length;
    int starting_point;
    int line;
    int col;
};

typedef struct TokenBuffer TokenBuffer;
struct TokenBuffer {
    Token* data;
    size_t count;
    size_t capacity;
};

Result lex_input(TokenBuffer* tokens, Source* source_file);

