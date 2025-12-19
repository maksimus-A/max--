#pragma once
#include <stddef.h>
#include <stdio.h>
#include "common.h"

// Tokens will be stored by their length, and pointer to
// their place in the file being lexed.

enum TokenKind {
    // ---- Special tokens ----
    TOK_EOF,

    // ---- Keywords ----
    IF,
    WHILE,
    FN,
    RETURN,
    INT,
    VOID,
    VARIABLE,

    // ---- Identifiers & literals ----
    IDENTIFIER,
    INT_LITERAL,

    // ---- Comments ----
    SL_COMMENT,
    ML_COMMENT,

    // ---- Delimiters / punctuation ----
    CUR_BRACK_START,
    CUR_BRACK_END,
    PAREN_START,
    PAREN_END,
    SEMICOLON,
    COLON,
    COMMA,

    // ---- Operators ----
    PLUS,
    MINUS,
    MULT,
    DIV,
    EQ,
    NEQ,
    LESS_THAN,

    // --- Error token ---
    NO_TOKEN,

    // ---- Token count (for debug) ----
    TOK_COUNT
};

typedef struct Token Token;
struct Token {
    enum TokenKind token_kind;
    size_t length;
    int start;
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

