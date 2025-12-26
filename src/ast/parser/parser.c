#include <stdalign.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "ast/lexer/lexer.h"
#include "ast/parser/parser.h"
#include "ast/parser/ast.h"
#include "arena/arena.h"
#include "errors/diagnostics.h"
#include "debug.h"
#include "common.h"

#define AST_DEFAULT_CAPACITY 32


void parse_item_list(Parser* parser, NodeList* list, Source* source_file, enum TokenKind stop_cond);
char* alloc_error_ptr(Parser* parser);

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
        /*
        LineCol line_col;
        line_col.line = current(parser).line;
        line_col.col = current(parser).col;*/

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
        case IDENTIFIER: // assignment statement
            if (next(parser).token_kind == EQ) return 1;
            break;
        default:
            return 0;
            break;
    }
}

// Compares name of IDENTIFIER to given string.
int compare_identifier_name(Parser* parser, Source* source_file, char* compare_to, size_t length) {
    if (current(parser).length != length) return 0;

    
    char* start_ptr = &source_file->buffer[current(parser).start];
    if (memcmp(start_ptr, compare_to, length) == 0) {
        return 1;
    }
    return 0;
}

int starts_builtin_func(Parser* parser, Source* source_file) {
    if (!(current(parser).token_kind == IDENTIFIER)) return 0;
    switch (current(parser).length) {
        case 4:
        {
            if (compare_identifier_name(parser, source_file, "exit", 4)) {
                if (next(parser).token_kind == PAREN_START) {
                    return 1;
                }
            }
            return 0;
        }

        default:
            return 0;
    }
    return 0;
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
        else if (current(parser).token_kind == CUR_BRACK_END) break;
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
// Converts integer literal string to actual integer
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
// parses expression, moves pointer to end of expr.
// Assumes pointer is at start of expression.
ASTNode* parse_expr(Parser* parser, Source* source_file) {
    ASTNode* expr = (ASTNode*)arena_alloc(parser->ast_arena, sizeof(ASTNode), alignof(ASTNode));

    if (current(parser).token_kind == INT_LITERAL) {
        // todo: parse -(unary op)
        expr->ast_kind = AST_INT_LIT;
        expr->node_info.int_lit = (IntLitInfo) {
            .value = get_int_lit_value(parser, source_file)
        };
        advance(parser);
    }
    else if (current(parser).token_kind == IDENTIFIER) {
        // TODO: Check entirety of expression.
        // For now we'll focus on single identifiers on RHS.
        expr->ast_kind = AST_NAME;
        expr->node_info.var_name.name_span = create_span_from(parser->tokens->data[parser->token_index].start, 
            parser->tokens->data[parser->token_index].start + parser->tokens->data[parser->token_index].length);
        advance(parser);
    }
    else { // error
        expr->ast_kind = AST_ERROR;
        add_err_msg(parser, "Could not parse expression.", current(parser).line, current(parser).col);
    }
    // TODO: Parse real expressions, not just integer literals.

    return expr;
}

ASTNode* parse_int_decl(Parser* parser, Source* source_file) {
    /*  Parses:
        int x; (not yet implemented)
        int x = {expr};
        Pointer assumed to be at 'INT'.
    */
    // TODO: Get the span of this declaration.
    ASTNode* int_decl = (ASTNode*)arena_alloc(parser->ast_arena, sizeof(ASTNode), alignof(ASTNode));
    int_decl->ast_kind = AST_VAR_DEC;

    char* err_msg = alloc_error(parser->diags);
    // Find identifier, store its span
    advance(parser);
    SrcSpan name_span;
    if (expect(parser, IDENTIFIER, err_msg)) {
        name_span = create_span_from(parser->tokens->data[parser->token_index-1].start, 
            parser->tokens->data[parser->token_index-1].start + parser->tokens->data[parser->token_index-1].length);
    }
    else {
        // Move pointer to next safe boundary
        int_decl->ast_kind = AST_ERROR;
        add_err_msg(parser, err_msg, current(parser).line, current(parser).col);
        sync_to_boundary(parser);
        return int_decl;
    }
    ASTNode* expr_node;
    // Check type of declaration (assignment or pure decl)
    if (match_kind(parser, EQ)) {
        expr_node = parse_expr(parser, source_file);
    }
    else if (current(parser).token_kind == SEMICOLON) {
        // i'll check if this is null to see if it was a
        // pure decl or expr decl.
        expr_node = NULL;
    }
    else {
        // TODO: Get specific identifier name and append to error list.
        int_decl->ast_kind = AST_ERROR;
        add_err_msg(parser, "Error: expected '=' or ';' after IDENTIFIER", current(parser).line, current(parser).col);
        expect_semicolon_or_recover(parser);
        return int_decl;
    }

    int_decl->node_info.var_decl = (struct VarDeclInfo){
        .name_span = name_span,
        .type = TYPE_INT,
        .init_expr = expr_node
    };
    
    char* err_msg_2 = arena_alloc(parser->ast_arena,
                            DEFAULT_ERR_MSG_SIZE,
                            alignof(char));
    if (!expect(parser, SEMICOLON, err_msg_2)) {
        int_decl->ast_kind = AST_ERROR;
        add_err_msg(parser, err_msg_2, current(parser).line, current(parser).col);
    }

    return int_decl;
}

// Parses statements/decls inside block. Assumes { was consumed.
ASTNode* parse_block_node(Parser* parser, Source* source_file) {
    ASTNode* block_node = (ASTNode*)arena_alloc(parser->ast_arena, sizeof(ASTNode), alignof(ASTNode));
    block_node->ast_kind = AST_BLOCK;

    block_node->node_info.block_info.body = (NodeList) {
        .items = (ASTNode**)arena_alloc(parser->ast_arena, AST_DEFAULT_CAPACITY * sizeof(ASTNode*), alignof(ASTNode*)),
        .capacity = AST_DEFAULT_CAPACITY,
        .count = 0
    };

    // Recursively fill items from while expr inside build_ast?
    // Make it a sub-routine?
    parse_item_list(parser, &block_node->node_info.block_info.body, source_file, CUR_BRACK_END);
    if (current(parser).token_kind == CUR_BRACK_END) advance(parser);
    else {
        add_err_msg(parser, "Expected '}'.", current(parser).line, current(parser).col);
        sync_to_boundary(parser);
    }

    return block_node;
}

// Parses builtin function 'exit(<expr>)'.
// Assumes we start on 'exit' token.
ASTNode* parse_exit(Parser* parser, Source* source_file) {

    ASTNode* exit_node = (ASTNode*)arena_alloc(parser->ast_arena, sizeof(ASTNode), alignof(ASTNode));
    exit_node->ast_kind = AST_EXIT;
    // TODO: Get span of the entire function (usually up to ;)
    // we already verified the next token is '('.
    advance(parser);
    char* err_msg = alloc_error_ptr(parser);
    expect(parser, PAREN_START, err_msg);

    ASTNode* expr = parse_expr(parser, source_file);
    exit_node->node_info.exit_info.expr = expr;

    // Pointer is after expression now
    char* err_msg_2 = alloc_error_ptr(parser);
    if (!expect(parser, PAREN_END, err_msg)) {
        add_err_msg(parser, err_msg_2, current(parser).line, current(parser).col);
        sync_to_boundary(parser);
        return exit_node;
    }
    char* err_msg_3 = alloc_error_ptr(parser);
    if (!expect(parser, SEMICOLON, err_msg_3)) {
        add_err_msg(parser, err_msg_3, current(parser).line, current(parser).col);
        sync_to_boundary(parser);
    }

    return exit_node;
}

// Parse assignment statement (x = 2;)
ASTNode* parse_assn(Parser* parser, Source* source_file) {
    // TODO: Get the span of this assn.
    ASTNode* assn_stmt = (ASTNode*)arena_alloc(parser->ast_arena, sizeof(ASTNode), alignof(ASTNode));
    assn_stmt->ast_kind = AST_ASSN;
    SrcSpan name_span;
    name_span = create_span_from(parser->tokens->data[parser->token_index].start, 
            parser->tokens->data[parser->token_index].start + parser->tokens->data[parser->token_index].length);

    char* err_msg = alloc_error(parser->diags);
    advance(parser);

    // ptr at expr
    if (expect(parser, EQ, err_msg)) {
        ASTNode* expr_node = parse_expr(parser, source_file);
        // todo: check name span is correct
        assn_stmt->node_info.assn_stmt = (struct AssnStmtInfo){
            .name_span = name_span,
            .type = TYPE_INT,
            .init_expr = expr_node
        };
    }
    else {
        // todo: check name_span is correct.
        assn_stmt->ast_kind = AST_ERROR;
        add_diag(parser->diags, ERROR, name_span, err_msg, current(parser).line, current(parser).col);
        sync_to_boundary(parser);
        return assn_stmt;
    }
    
    char* err_msg_2 = alloc_error(parser->diags);
    if (!expect(parser, SEMICOLON, err_msg_2)) {
        assn_stmt->ast_kind = AST_ERROR;
        add_diag(parser->diags, ERROR, name_span, err_msg, current(parser).line, current(parser).col);
    }
    return assn_stmt;
}

/*------ ITEM HELPER ------ */
void ensure_item_capacity(Parser* parser, NodeList* list) {
    if (list->capacity == list->count) {
        size_t new_capacity = list->capacity * 2;
        ASTNode** new_items = (ASTNode**)arena_alloc(parser->ast_arena, new_capacity * sizeof(ASTNode*), alignof(ASTNode*));
        if (!new_items) {
            // TODO: Error handle better.
            fprintf(stderr, "ERROR: Could not allocate new items list for 'program' from arena.");
            return;
        }
        if (list->count > 0) {
            memcpy(new_items, list->items, list->count * sizeof(ASTNode*));
        }
        list->capacity = new_capacity;
        list->items = new_items;
        return;
    }
}

void push_node(Parser* parser, NodeList* list, ASTNode* node) {
    // TODO: Handle errors pushing to list!
    ensure_item_capacity(parser, list);

    size_t count = list->count;
    list->items[count] = node;
    list->count++;
}

/*------ ERROR MESSAGE HELPERS ------*/
// todo: replace all of these with Diagnostics struct
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

// True if errors were present.
int print_parser_err_msgs(Parser* parser) {
    if (parser->error_list_size == 0) return 0;
    fprintf(stdout, "Parser errors:\n");
    for (int i = 0; i < parser->error_list_size; i++) {
        fprintf(stdout, "%s\n", parser->error_list[i]);
    }
    return 1;
}

// Allocates memory for an error message (if we need it).
char* alloc_error_ptr(Parser* parser) {
    return arena_alloc(parser->ast_arena,
                    DEFAULT_ERR_MSG_SIZE,
                    alignof(char));
}

void parse_item_list(Parser* parser, NodeList* list, Source* source_file, enum TokenKind stop_cond) {

    while (current(parser).token_kind != stop_cond && current(parser).token_kind != TOK_EOF) {
        if (starts_decl(parser)) {
            switch (current(parser).token_kind) {
                case INT:
                    // TODO: Just call 'parse_int_decl' and let that handle missing identifier.
                    if (next(parser).token_kind == IDENTIFIER) {
                        ASTNode* int_decl = parse_int_decl(parser, source_file);
                        push_node(parser, list, int_decl);
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
        else if (starts_stmt(parser)) {
            switch (current(parser).token_kind) {
                case IDENTIFIER:
                {
                    if (next(parser).token_kind == EQ) {
                        if (peek_n(parser, 2).token_kind == EQ) {
                            // todo: later consider conditionals, '=='
                        }
                        else {
                            ASTNode* assn = parse_assn(parser, source_file);
                            push_node(parser, list, assn);
                        }
                    }
                    break;
                }

                default: break;
            }

        }
        else if (starts_builtin_func(parser, source_file)) {
            if (compare_identifier_name(parser, source_file, "exit", 4)) {
                ASTNode* builtin_exit = parse_exit(parser, source_file);
                push_node(parser, list, builtin_exit);
            }
        }
        else if (current(parser).token_kind == CUR_BRACK_START) {
            advance(parser);
            ASTNode* block_node = parse_block_node(parser, source_file);
            push_node(parser, list, block_node);
        }
        else {
            add_err_msg(parser, "Unexpected token.", current(parser).line, current(parser).col);
            //TODO* Remove this once more things r implemented.
            // I want each 'parse_x' to advance the pointer to proper place
            parser->token_index++;
        }

        // todo: Error checking after each built node
        /*I define parse_{node} for each node. */
    }
}

ASTNode* build_ast(Parser* parser, Source* source_file) {

    ASTNode* ast_root = (ASTNode*)arena_alloc(parser->ast_arena, sizeof(ASTNode), alignof(ASTNode));

    ast_root->ast_kind = AST_PROGRAM;
    ast_root->span = create_span_from(parser->token_index, parser->tokens->count-1);
    ast_root->node_info.program.body = (NodeList) {
        .items = (ASTNode**)arena_alloc(parser->ast_arena, AST_DEFAULT_CAPACITY * sizeof(ASTNode*), alignof(ASTNode*)),
        .capacity = AST_DEFAULT_CAPACITY,
        .count = 0
    };
    parse_item_list(parser, &ast_root->node_info.program.body, source_file, TOK_EOF);

    return ast_root;
}


int initialize_parser(Parser* parser, Arena* arena, TokenBuffer* tokens, Diagnostics* diags) {
    parser->ast_arena = arena;
    parser->token_index = 0;
    parser->tokens = tokens;
    parser->diags = diags;
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
