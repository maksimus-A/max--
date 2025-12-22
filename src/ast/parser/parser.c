#include <stdalign.h>
#include <stdlib.h>
#include <assert.h>
#include "ast/lexer/lexer.h"
#include "ast/parser/parser.h"
#include "arena/arena.h"

/* Peeks n tokens ahead (or behind?) and returns the token, unless EOF.*/
Token peek_n(Parser* parser, int n) {
    int token_index = parser->token_index;

    // TODO: Make this error check more robust
    assert(0 <= token_index && token_index < (int)parser->tokens->count);
    if (0 <= token_index + n && token_index + n < (int)parser->tokens->count) {
        return parser->tokens->data[token_index+n];
    }
    return parser->tokens->data[parser->tokens->count-1];
}

/* Get next token in tokens */
Token next(Parser* parser) {
    return peek_n(parser, 1);
}

/* Grab current token in tokens */
Token current(Parser* parser) {
    return peek_n(parser, 0);
}

// Advance our token pointer by n tokens (or EOF)
void advance_n(Parser* parser, int n) {
    int token_index = parser->token_index;
    int total_token_count = parser->tokens->count;
    if (token_index + n < total_token_count) {
        parser->token_index += n;
    }
    parser->token_index = parser->tokens->count-1;
    // TODO: Return some error msg? Or that we reached EOF?
}

// Returns true if tokens match, and consumes token
int match_kind(Parser* parser, enum TokenKind kind) {
    if (parser->token_index >= (int)parser->tokens->count-1) {
        return 0;
    }

    if (parser->tokens->data[parser->token_index].token_kind == kind) {

        parser->token_index++;
        return 1;
    }
    return 0;
}

// Returns true if token matches one of the tokens given in the list, and consumes token
int match_one_of_kinds(Parser* parser, enum TokenKind* kind, int size) {
    if (parser->token_index >= (int)parser->tokens->count-1) {
        return 0;
    }
    for (int j = 0; j < size; j++) {
        if (parser->tokens->data[parser->token_index].token_kind == kind[j]) {
            parser->token_index++;
            return 1;
        }
    }
    return 0;
}

// True if current token starts a declaration
int starts_decl(Parser* parser) {
    switch (current(parser).token_kind) {
        // TODO: Add rest of declarations.
        // Consider making a 'TYPE' that
        // encapsulates all types as checks.
        case INT:
            return 1;
            break;
        case FN:
            return 1;
            break;
        default:
            return 0;
            break;
    }
}

// True if current token starts a statement
int starts_stmt(Parser* parser) {
    switch (current(parser).token_kind) {
        // TODO: Add rest of statements.
        // Consider making a 'TYPE' that
        // encapsulates all types as checks.
        case IF:
            return 1;
            break;
        case WHILE:
            return 1;
            break;
        case RETURN:
            return 1;
            break;
        case CUR_BRACK_START: // block start
            return 1;
            break;
        default:
            return 0;
            break;
    }
}

// Create a span struct for metadata storage
SrcSpan create_span_from(int i, int start_mark) {
    SrcSpan span;
    span.start = start_mark;
    span.length = i - start_mark;

    return span;
}

/*Help recover from syntax errors by syncing to
 closest statment boundary or EOF.*/
int sync_to_boundary(Parser* parser) {
    while (parser->token_index < (int)parser->tokens->count-1) {
        if (starts_stmt(parser)) break;
        else if (match_kind(parser, CUR_BRACK_END)) {
            // don't consume, go to right before
            // so block parser parses it properly
            parser->token_index--;
            break;
        }
        else if (match_kind(parser, SEMICOLON)) break;
        parser->token_index++;
    }
    return parser->token_index;
}

/*Help recover from syntax errors by syncing to
 closest semicolon or EOF.*/
int expect_semicolon_or_recover(Parser* parser) {
    while (parser->token_index < (int)parser->tokens->count-1) {
        if (match_kind(parser, SEMICOLON)) break;
        parser->token_index++;
    }
    return parser->token_index;
}

// TODO: Since THIS is allocating the arena, this should also probably free it later.
// I think that makes the most sense.
ASTNode* build_ast(Parser* parser) {

    ASTNode* ast_root = (ASTNode*)arena_alloc(parser->ast_arena, choose_block_size(sizeof(ASTNode)), alignof(ASTNode));

    
    /*
    Idea:
    recursive descent. Start with visit_program()
    visit_program:
        visits every single subtype inside of ASTKind
        Reads syntax
        Determines what AstKind and assign
        Stores buffer length + start
        Store metadata
        If any metadata requires a recursive call:
        Recursively call
    
    */

}


int initialize_parser(Parser* parser, Arena* arena, TokenBuffer* tokens) {
    // TODO: Error check this?
    // but tbh i don't see how it could fail.
    parser->ast_arena = arena;
    parser->token_index = 0;
    parser->tokens = tokens;
    // TODO: Error list init
    return 1;
}

int free_ast_arena(Parser* parser) {
    // TODO: Implement this
    return 0;
}
