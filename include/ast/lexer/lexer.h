#pragma once
#include <stddef.h>
#include <stdio.h>

// Tokens will be stored by their length, and pointer to
// their place in the file being lexed.

enum TokenKind {
    IF,
    WHILE,
    PLUS,
    MINUS,
    MULT,
    DIV
};

typedef struct Token Token;
struct Token {
    enum TokenKind token_kind;
    size_t length;
    int starting_point;
    int line;
    int col;
};
