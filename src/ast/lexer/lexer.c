#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "common.h"
#include "ast/lexer/lexer.h"

static inline int is_ident_starter(char c) {
    return isalpha((unsigned char)c) || c == '_';
}

static inline int is_ident_char(char c) {
    return isalnum((unsigned char)c) || c == '_';
}

static inline int is_whitespace_char(char c) {
    return c == '\n' || c == ' ' || c == '\t';
}


static int get_whitespace_len(char* buf, int i) {
    int start = i;
    while (is_whitespace_char(buf[i]) & (buf[i] != '\0')) i++;
    return i - start;
}

static int get_sl_comment_len(char* buf, int i) {
    // Assumes we know we're in a comment.
    // Moves index pointer to after the comment.
    int start = i;
    while (buf[i] != '\n' & buf[i] != '\0') i++;
    return i - start;
}

static int get_digit_length(char* buf, int i) {
    int start = i;
    while (isdigit((unsigned char)buf[i])) i++;
    return i - start;
}

static int get_identifier_len(char* buf, int i) {
    // Checks if the following word is any
    // identifier. I'll check keywords after.
    // returns index pointer to end of identifier.
    int start = i;
    if (is_ident_starter(buf[i])) {
        i++;
        while (is_ident_char(buf[i])) i++;
    }
    return i - start;
}


Token set_token(enum TokenKind type, int start, int line, int col, int length) {
    Token token;
    token.token_kind = type;
    token.start = start;
    token.length = length;
    token.line = line;
    token.col = col;

    return token;
}

void ensure_capacity(TokenBuffer* tokens) {
    if (tokens->capacity == tokens->count) {
        size_t new_capacity = tokens->capacity * 2;
        Token* new_data = realloc(tokens->data, sizeof(Token) * new_capacity);
        if (!new_data) {
            // TODO: Error handle better.
            fprintf(stderr, "ERROR: Could not re-allocate buffer.");
        }
        tokens->data = new_data;
        tokens->capacity = new_capacity;
    }
}

void push_token(TokenBuffer* tokens, Token token) {
    // TODO: Handle error pushing tokens!
    ensure_capacity(tokens);
    tokens->data[tokens->count] = token;
    tokens->count++;
}

Result lex_input(TokenBuffer* tokens, Source* source_file) {

    // idea:
    // go char by char
    // if i see the start of a keyword:
    // check if it is the keyword
    // if it is, store its line, col,
    // length, and start.

    Result result;
    result.error_code = 0;
    result.error_message = "";

    int i = 0;
    int line = 1;
    int col = 0;

    while (i < source_file->length) {
        char c = source_file->buffer[i];
        // TODO: SL comment skipper, keyword finder, func finder?
        // FOR NOW WE TRY TO SUPPORT:
        // tests/testfiles/simple/int_dec.in
        // tests/testfiles/strings/hello_world.in
        switch (c) {
            case '=':
                {
                    Token token = set_token(EQ, i, line, col, 1);
                    push_token(tokens, token);
                }
                break;
            case ';':
                {
                    Token token = set_token(SEMICOLON, i, line, col, 1);
                    push_token(tokens, token);
                }
                break;
            case '(':
                {
                    Token token = set_token(PAREN_START, i, line, col, 1);
                    push_token(tokens, token);
                }
                break;
            case ')':
                {
                    Token token = set_token(PAREN_END, i, line, col, 1);
                    push_token(tokens, token);
                }
                break;
            case '\n':
                {
                    line++;
                    col = 0;
                }
                break;

            default:
                if (isdigit((unsigned char)c)) {
                    int length = get_digit_length(source_file->buffer, i);
                    Token token = set_token(INT_LITERAL, i, line, col, length);
                    push_token(tokens, token);
                    i = i + length - 1;
                }
                else if (is_ident_starter((unsigned char)c)) {
                    int length = get_identifier_len(source_file->buffer, i);
                    Token token = set_token(IDENTIFIER, i, line, col, length);
                    push_token(tokens, token);
                    i = i + length - 1;
                }
                else if (is_whitespace_char((unsigned char)c)){
                    // skip whitespace
                    int length = get_whitespace_len(source_file->buffer, i);
                    i = i + length - 1;
                }

                break;


        }
        col++;
        i++;
    }
    return result;
}

