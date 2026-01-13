#include "ast/parser/ast.h"
#include "errors/diagnostics.h"
#include "semantics/def-assn-analysis/def_assn.h"
#include "semantics/walker.h"
#include <assert.h>
#include <stdalign.h>
#include <stdint.h>
#include <string.h>

// Checks if the bit in the bitset pertaining to the 'id' of the
// local variable is set to 1 (implying it was assigned/initialized).
bool is_assigned(uint64_t* assigned, size_t var_id) {
    size_t word_index = var_id / 64;
    uint64_t bit_index = var_id % 64;

    uint64_t mask = (uint64_t)1 << bit_index;
    uint64_t is_set = ((assigned[word_index] & mask) != 0);
    return is_set;
}

void assign_variable(uint64_t* assigned, size_t var_id) {
    size_t word_index = var_id / 64;
    uint64_t bit_index = var_id % 64;

    uint64_t mask = (uint64_t)1 << bit_index;

    assigned[word_index] |= mask;
}


WalkChildren def_assn_pre(void* user, ASTNode* node) {
    DefAssn* defassn = (DefAssn*)user;

    switch (node->ast_kind) {
        case AST_NAME:
        {
            size_t child_expr_id = node->node_info.var_name.resolved_sym->id;
            if (!is_assigned(defassn->assigned, child_expr_id)) {
                SrcSpan var_span = node->node_info.var_name.name_span;
                create_and_add_diag_fmt(defassn->diags, ERROR, var_span, 
                    "Symbol '%.*s' was not assigned/initialized before use.", defassn->source_file);
            }
            break;
        }
        default: break;
    }
    return WALK_CHILDREN;
}

void def_assn_post(void* user, ASTNode* node) {
    DefAssn* defassn = (DefAssn*)user;
    
    switch (node->ast_kind) {
        case AST_VAR_DEC: 
        {
            if (node->node_info.var_decl.init_expr != NULL) {
                size_t var_id = node->node_info.var_decl.symbol->id;
                assign_variable(defassn->assigned, var_id);
            }
            break;
        }
        case AST_ASSN:
        {
            size_t var_id = node->node_info.assn_stmt.resolved_sym->id;
            assign_variable(defassn->assigned, var_id);
            break;
        }
        default: break;
    }
    
    if (defassn->debug) dump_assigned_bits(defassn);
}

Visitor def_assn_visitor = {
    .pre = def_assn_pre,
    .post = def_assn_post
};

void run_definite_assignment(ASTNode* ast_root, DefAssn* defassn) {
    walk_node(&def_assn_visitor, defassn, ast_root);
}

size_t words_for_locals(size_t total_locals) {
    return (total_locals + 63) / 64;
}

bool definite_assignment_init(DefAssn* defassn, Diagnostics* diags, Arena* arena, Source* source_file, bool debug, size_t total_locals) {
    defassn->arena = arena;
    defassn->diags = diags;
    defassn->source_file = source_file;
    defassn->debug = debug;

    // Calculate words needed and assign
    size_t words_needed = words_for_locals(total_locals);
    defassn->assigned = arena_alloc(arena, words_needed * sizeof(uint64_t), alignof(uint64_t));
    memset(defassn->assigned, 0, words_needed * sizeof(uint64_t));

    defassn->assigned_size = words_needed;
    return true;
}

void print_bits(uint64_t value, size_t j) {
    size_t num_bits = 64;

    printf("Assigned (bin), index %zu: ", j);
    for (size_t i = num_bits - 1; i-- > 0; ) {
        // Create a mask with the i-th bit set to 1
        uint64_t mask = (uint64_t)1 << i;
        
        // Use bitwise AND (&) to check if the i-th bit of 'value' is set
        // If the result is non-zero (true), print 1, otherwise print 0
        if (value & mask) {
            printf("1");
        } else {
            printf("0");
        }

        // Optional: print a space after every 8 bits for readability
        if (i % 8 == 0) {
            printf(" ");
        }
    }
    printf("\n");
}

void dump_assigned_bits(DefAssn* defassn) {

    size_t arr_size = defassn->assigned_size;
    for (size_t i = 0; i < arr_size; i++) {
        print_bits(defassn->assigned[i], i);
    }
    printf("\n");
}

