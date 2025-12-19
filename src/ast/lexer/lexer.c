#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "common.h"
#include "ast/lexer/lexer.h"

// TODO: Consider putting helpers inside 'lexer_helper.c' or something

static inline int is_ident_starter(char c) {
    return isalpha((unsigned char)c) || c == '_';
}

static inline int is_ident_char(char c) {
    return isalnum((unsigned char)c) || c == '_';
}

static inline int is_whitespace_char(char c) {
    return c == '\n' || c == ' ' || c == '\t';
}

static inline int is_comment_or_div_starter(char* buf, int i) {
    // For both single/multi-line and division op
    if (buf[i] == '/') {
        return (buf[i+1] == '/' || buf[i+1] == '*' || is_whitespace_char(buf[i+1]));
    }
    return 0;
}

static inline int is_sl_comment(char* buf, int i) {
    return buf[i+1] == '/';
}

static inline int is_ml_comment(char* buf, int i) {
    if (buf[i] != '\0') {
        return buf[i+1] == '*';
    }
    // TODO: Error handle; if we get here the the ML comment reached EOF.
    // Need to refactor a lot for error handling. Sounds cringe.
    return 0;
}

static inline int is_div_op(char* buf, int i) {
    return is_whitespace_char(buf[i+1]);
}

static enum TokenKind get_keyword(char* buf, int i, int length) {
    enum TokenKind token_kind = IDENTIFIER;
    char *p = buf + i;
    switch (length) {
        case 2:
            if (memcmp(p, "if", 2) == 0) token_kind = IF;
            else if (memcmp(p, "fn", 2) == 0) token_kind = FN;
            break;

        case 3:
            if (memcmp(p, "int", 3) == 0) token_kind = INT;
            break;

        case 4:
            if (memcmp(p, "void", 4) == 0) token_kind = VOID;
            break;
        
        case 5:
            if (memcmp(p, "while", 5) == 0) token_kind = WHILE;
            break;

        case 6:
            if (memcmp(p, "return", 6) == 0) token_kind = RETURN;
            break;

        default:
            token_kind = IDENTIFIER;
    }
    return token_kind;
}

static int get_whitespace_len(char* buf, int i) {
    int start = i;
    while (is_whitespace_char(buf[i]) && (buf[i] != '\0')) i++;
    return i - start;
}

static int get_sl_comment_len(char* buf, int i) {
    // Assumes we know we're in a comment.
    // Moves index pointer to after the comment.
    int start = i;
    while (buf[i] != '\n' && buf[i] != '\0') i++;
    return i - start;
}

static int get_ml_comment_len(char* buf, int i) {
    // Assumes we know we're in a comment.
    // Moves index pointer to after the comment.
    int start = i;
    while (buf[i] != '\0')  {
        if (buf[i] == '*' && buf[i+1] == '/') return i+1-start;
        i++;
    }
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
    // TODO: Handle errors pushing tokens!
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

    // TODO: actually use result or just refactor its use case
    // TODO: Fix col bug, when it's a newline, col is incorrect (up by 1).

    Result result;
    result.error_code = 0;
    result.error_message = "";

    int i = 0;
    int line = 1;
    int col = 0;

    while (i < source_file->length) {
        char c = source_file->buffer[i];
        // TODO: keyword finder, func finder?
        // FOR NOW WE TRY TO SUPPORT:
        // tests/testfiles/simple/int_dec.in
        // tests/testfiles/strings/hello_world.in
        switch (c) {
            // Simple delimiters
            case '=':
                {
                    // TODO: Consider '==' as well.
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
            case ',':
                {
                    Token token = set_token(COMMA, i, line, col, 1);
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
            case '{':
                {
                    Token token = set_token(CUR_BRACK_START, i, line, col, 1);
                    push_token(tokens, token);
                }
                break;
            case '}':
                {
                    Token token = set_token(CUR_BRACK_END, i, line, col, 1);
                    push_token(tokens, token);
                }
                break;
            case ':':
                {
                    Token token = set_token(COLON, i, line, col, 1);
                    push_token(tokens, token);
                }
                break;
            // Operations
             case '+':
                {
                    Token token = set_token(PLUS, i, line, col, 1);
                    push_token(tokens, token);
                }
                break;
            case '-':
                {
                    Token token = set_token(MINUS, i, line, col, 1);
                    push_token(tokens, token);
                }
                break;
            case '*':
                {
                    // TODO: Check for exponentiation
                    Token token = set_token(MULT, i, line, col, 1);
                    push_token(tokens, token);
                }
                break;
            case '<':
                {
                    Token token = set_token(LESS_THAN, i, line, col, 1);
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
                    Token token;
                    enum TokenKind token_kind = get_keyword(source_file->buffer, i, length);
                    // TODO: Validate identifier if token_kind is not a keyword
                    // Currently doesn't validate if no invalid keywords exist.
                    token = set_token(token_kind, i, line, col, length);
                    push_token(tokens, token);
                    i = i + length - 1;
                }
                else if (is_whitespace_char((unsigned char)c)){
                    // skip whitespace
                    int length = get_whitespace_len(source_file->buffer, i);
                    i = i + length - 1;
                }
                else if (is_comment_or_div_starter(source_file->buffer, i)) {
                    // skip single line comments
                    // TODO: Skip multi-line here too later, shouldn't be hard.
                    if (is_div_op(source_file->buffer, i)) {
                        Token token = set_token(DIV, i, line, col, 1);
                        push_token(tokens, token);
                    }
                    else if (is_sl_comment(source_file->buffer, i)) {
                        int length = get_sl_comment_len(source_file->buffer, i);
                        i = i + length - 1;
                    }
                    else if (is_ml_comment(source_file->buffer, i)) {
                        int length = get_ml_comment_len(source_file->buffer, i);
                        i = i + length - 1;
                    }
                    
                }
                break;


        }
        col++;
        i++;
    }
    return result;
}

