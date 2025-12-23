#include <stdalign.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "ast/lexer/lexer.h"
#include "ast/parser/parser.h"
#include "ast/parser/ast.h"
#include "arena/arena.h"
#include "debug.h"
#include "common.h"

#define AST_DEFAULT_CAPACITY 32
#define DEFAULT_ERR_MSG_SIZE 100

/*--------- HELPERS --------- */
// Checkers
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

/* Get next token in tokens. Does not advance token index. */
Token next(Parser* parser) {
    return peek_n(parser, 1);
}

/* Grab current token in tokens, does not advance token index */
Token current(Parser* parser) {
    return peek_n(parser, 0);
}

// Consumers
/* Returns true if CURRENT token matches expectation (& consume), false otherwise.
 Used when next token is mandatory.*/
int expect(Parser* parser, enum TokenKind kind, char* err_msg) {
    if (current(parser).token_kind == kind) {
        parser->token_index++;
        return 1;
    }
    else {
        // Error handling by returning expected tokens.
        int curr_index = parser->token_index;

        assert((size_t)curr_index + 1 <= parser->tokens->count);
        enum TokenKind curr_token = parser->tokens->data[curr_index].token_kind;
        const char* curr_token_str = token_kind_str[curr_token];
        const char* expected_token_str = token_kind_str[kind];

        LineCol line_col;
        line_col.line = current(parser).line;
        line_col.col = current(parser).col;

        // Todo: Check which token was expected.
        int res = snprintf(err_msg, DEFAULT_ERR_MSG_SIZE, "Expected %s after %s", expected_token_str, curr_token_str);
        return 0;
    }
}

// Advance our token pointer by n tokens (or EOF)
int advance_n(Parser* parser, int n) {
    int token_index = parser->token_index;
    int total_token_count = parser->tokens->count;
    if (token_index + n < total_token_count) {
        parser->token_index += n;
        return 1;
    }
    else {
        parser->token_index = parser->tokens->count-1;
        return 0;
    }
    // TODO: Use T/F returned from this
}

// Advance token pointer by 1 token (or to EOF)
int advance(Parser* parser) {
    return advance_n(parser, 1);
}

// Returns true if CURRENT tokens match, and consumes token. 
// Used when next token is optional.
int match_kind(Parser* parser, enum TokenKind kind) {
    if (parser->token_index >= (int)parser->tokens->count-1) {
        return 0;
    }

    if (current(parser).token_kind == kind) {
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
        if (current(parser).token_kind == kind[j]) {
            parser->token_index++;
            return 1;
        }
    }
    return 0;
}

/*--------- VISIT_[NODE] --------- */


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
SrcSpan create_span_from(int start_mark, int end_mark) {
    SrcSpan span;
    span.start = start_mark;
    span.length = end_mark - start_mark;

    return span;
}

/*--------- ERROR RECOVERY ---------*/
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
    // TODO: This should probably return an error since we reached EOF.
    return parser->token_index;
}

/*--------- PARSING HELPERS ---------*/

long get_int_lit_value(Parser* parser, Source* source_file) {
    // TODO: Maybe move into lexer?
    int index = parser->token_index;
    int start = parser->tokens->data[index].start;
    int end = start + parser->tokens->data[index].length;

    long value = 0;
    for (int i = start; i < end; i++) {
        char c = source_file->buffer[i];
        if (c < '0' || c > '9') {
            // error: non-digit in integer literal
            value = -1;
            break;
        }
        int digit = c - '0';
        // TODO: Add overflow check
        value = value * 10 + digit;
    }

    return value;
}

/*--------- PARSE_{NODE} ---------*/
ASTNode* parse_expr(Parser* parser, Source* source_file) {
    // TODO*: Move token_index to end of expression.
    ASTNode* expr = (ASTNode*)arena_alloc(parser->ast_arena, sizeof(ASTNode), alignof(ASTNode));

    if (current(parser).token_kind == INT_LITERAL) {
        // TODO: Decide if this is AST_INT_LIT or AST_EXPR.
        expr->ast_kind = AST_INT_LIT;
        expr->node_info.int_lit = (IntLitInfo) {
            .value = get_int_lit_value(parser, source_file)
        };
        advance(parser);
    }
    // TODO: Parse real expressions, not just integer literals.

    return expr;
}

ASTNode* parse_int_decl(Parser* parser, Source* source_file) {
    /*  Parses:
        int x;
        int x = {expr};
        Pointer assumed to be at 'INT'.
    */

    ASTNode* int_decl = (ASTNode*)arena_alloc(parser->ast_arena, sizeof(ASTNode), alignof(ASTNode));
    int_decl->ast_kind = AST_VAR_DEC;

    // TODO: Append err message to err message list in parser
    char* err_msg = arena_alloc(parser->ast_arena,
                            DEFAULT_ERR_MSG_SIZE,
                            alignof(char));
    // Find identifier, store its span
    advance(parser);
    SrcSpan name_span;
    if (expect(parser, IDENTIFIER, err_msg)) {
        name_span = create_span_from(parser->tokens->data[parser->token_index-1].start, 
            parser->tokens->data[parser->token_index-1].start + parser->tokens->data[parser->token_index-1].length);
    }
    else {
        // Move pointer to next safe boundary
        // TODO: Error check harder?
        add_err_msg(parser, err_msg, current(parser).line, current(parser).col);
        expect_semicolon_or_recover(parser);
        return int_decl;
    }

    // Check type of declaration (assignment or pure decl)
    if (match_kind(parser, EQ)) {
        ASTNode* expr_node = parse_expr(parser, source_file);

        int_decl->node_info.var_decl = (struct VarDeclInfo){
            .name_span = name_span,
            .type = TYPE_INT,
            .init_expr = expr_node
        };
    }
    else {
        // TODO: Get specific identifier name and append to error list.
        add_err_msg(parser, "Error: expected '=' after IDENTIFIER", current(parser).line, current(parser).col);
        expect_semicolon_or_recover(parser);
        return int_decl;
    }
    
    char* err_msg_2 = arena_alloc(parser->ast_arena,
                            DEFAULT_ERR_MSG_SIZE,
                            alignof(char));
    if (!expect(parser, SEMICOLON, err_msg_2)) {
        add_err_msg(parser, err_msg_2, current(parser).line, current(parser).col);
    }

    return int_decl;
}

/*------ ITEM HELPER ------ */
void ensure_item_capacity(Parser* parser, ASTNode* ast_root) {
    if (ast_root->node_info.program.capacity == ast_root->node_info.program.count) {
        size_t new_capacity = ast_root->node_info.program.capacity * 2;
        ASTNode** new_items = (ASTNode**)arena_alloc(parser->ast_arena, new_capacity * sizeof(ASTNode*), alignof(ASTNode*));
        if (!new_items) {
            // TODO: Error handle better.
            fprintf(stderr, "ERROR: Could not allocate new items list for 'program' from arena.");
            return;
        }
        if (ast_root->node_info.program.count > 0) {
            memcpy(new_items, ast_root->node_info.program.items, ast_root->node_info.program.count * sizeof(ASTNode*));
        }
        ast_root->node_info.program.capacity = new_capacity;
        ast_root->node_info.program.items = new_items;
        return;
    }
}

void push_node(Parser* parser, ASTNode* ast_root, ASTNode* node) {
    // TODO: Handle errors pushing ast_root!
    ensure_item_capacity(parser, ast_root);

    size_t count = ast_root->node_info.program.count;
    ast_root->node_info.program.items[count] = node;
    ast_root->node_info.program.count++;
}

/*------ ERROR MESSAGE HELPERS ------*/

// Adds error message with line/col to error list during parsing.
void add_err_msg(Parser* parser, char* err_msg, size_t line, size_t col) {
    char* new_err_msg = arena_alloc(parser->ast_arena,
                            DEFAULT_ERR_MSG_SIZE,
                            alignof(char));
    snprintf(new_err_msg, DEFAULT_ERR_MSG_SIZE, "Error at (%zu:%zu): %s", line, col, err_msg);
    if (parser->error_list_size < DEFAULT_ERROR_LIST_SIZE) {
        parser->error_list[parser->error_list_size] = new_err_msg;
        parser->error_list_size++;
    }
}

// Adds 

// True if errors were present.
int print_parser_err_msgs(Parser* parser) {
    if (parser->error_list_size == 0) return 0;
    fprintf(stdout, "Parser errors:\n");
    for (int i = 0; i < parser->error_list_size; i++) {
        fprintf(stdout, "%s\n", parser->error_list[i]);
    }
    return 1;
}


// TODO: Since THIS is allocating the arena, this should also probably free it later.
// I think that makes the most sense.
ASTNode* build_ast(Parser* parser, Source* source_file) {

    ASTNode* ast_root = (ASTNode*)arena_alloc(parser->ast_arena, sizeof(ASTNode), alignof(ASTNode));

    ast_root->ast_kind = AST_PROGRAM;
    ast_root->span = create_span_from(parser->token_index, parser->tokens->count-1);
    ast_root->node_info.program = (ProgramInfo) {
        .items = (ASTNode**)arena_alloc(parser->ast_arena, AST_DEFAULT_CAPACITY * sizeof(ASTNode*), alignof(ASTNode*)),
        .capacity = AST_DEFAULT_CAPACITY,
        .count = 0
    };
    while (parser->tokens->data[parser->token_index].token_kind != TOK_EOF) {
        int index = parser->token_index;
        if (starts_decl(parser)) {
            switch (parser->tokens->data[index].token_kind) {
                case INT:
                    // TODO: Just call 'parse_int_decl' and let that handle missing identifier.
                    if (next(parser).token_kind == IDENTIFIER) {
                        ASTNode* int_decl = parse_int_decl(parser, source_file);
                        push_node(parser, ast_root, int_decl);
                    }
                    else {
                        add_err_msg(parser, "Expected 'IDENTIFIER' after 'int'.", current(parser).line, current(parser).col);
                        expect_semicolon_or_recover(parser);
                    }
                    break;
                default:
                    break;
            }
        }
        else {
            //TODO* Remove this once more things r implemented.
            // I want each 'parse_x' to advance the pointer.
            parser->token_index++;
        }
        /*I define parse_{node} for each node. */
        
    }
    return ast_root;
}


int initialize_parser(Parser* parser, Arena* arena, TokenBuffer* tokens) {
    parser->ast_arena = arena;
    parser->token_index = 0;
    parser->tokens = tokens;
    parser->error_list_size = 0;
    return 1;
}

int free_ast_arena(Parser* parser) {
    if (!arena_destroy(parser->ast_arena)) {
        fprintf(stderr, "Failed to destroy AST arena.");
        return 1;
    }
    return 0;
}
