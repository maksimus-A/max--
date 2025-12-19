#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "common.h"
#include "ast/lexer/lexer.h"

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
    int line = 0;
    int col = 0;
    // todo: keep track of line/col!
    while (i < source_file->length) {
        char c = source_file->buffer[i];
        // FOR NOW WE SUPPORT:
        // tests/testfiles/simple/int_dec.in
        // tests/testfiles/strings/hello_world.in
        switch (c) {
            case 'i':
                break;

        }
    }
    return result;
}

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
    while (is_whitespace_char(buf[i])) i++;
    return i - start;
}

static int get_sl_comment_len(char* buf, int i) {
    // Assumes we know we're in a comment.
    // Moves index pointer to after the comment.
    int start = i;
    while (buf[i] != '\n') i++;
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
