#include <stdio.h>
#include <stdlib.h>
#include "ast/parser/ast.h"
#include "common.h"

char* built_in_type_string[TYPE_TOTAL_COUNT] = {
    /*TYPE_INT*/         "int", // Converts to i32

    // Signed integers
    /*TYPE_SI64*/        "i64",

    /*TYPE_BOOL*/        "bool",
    /*TYPE_CHAR*/        "char",
};

const char* op_string[TOK_COUNT] = {
    [PLUS] = "+",
    [MINUS] = "-",
    [MULT] = "*",
    [DIV] = "/",
    [EQQ] = "==",
    [NEQ] = "!=",
    [LESS_THAN] = "<"
};

char* get_type_string(BuiltInType type) {
    return built_in_type_string[type];
}



// Prints indents equal to number of indents.
void print_indentation(int indent) {
    for (int i = 0; i < indent; i++) {
        printf("  ");
    }
}

/*
(Program
  (VarDecl type=int name=x
    (Int_Lit 123)
    )
)*/
void dump_ast(ASTNode* node, Source* source_file, int indent) {
    print_indentation(indent);
    printf("(");
    switch (node->ast_kind) {
        case AST_PROGRAM:
        {
            printf("Program id={%d}\n", (int)node->id);
            indent++;
            for (size_t i = 0; i < node->node_info.program.body.count; i++) {
                dump_ast(node->node_info.program.body.items[i], source_file, indent);
            }
            indent--;
            print_indentation(indent);
            printf(")\n");
            break;
        }
        case AST_VAR_DEC: 
        {
            char* start_ptr = start_of_name(node->node_info.var_decl.name_span, source_file);

            printf("VarDecl id={%d} type={%s} name={", (int)node->id, get_type_string(node->node_info.var_decl.type));
            print_file_slice(start_ptr, node->node_info.var_decl.name_span.length, stdout);
            printf("}");
            
            if (node->node_info.var_decl.init_expr != NULL) {
                printf("\n");
                indent++;
                dump_ast(node->node_info.var_decl.init_expr, source_file, indent);
                indent--;
                print_indentation(indent);
            }
            printf(")\n");

            break;
        }
        case AST_BIN_OP:
        {
            printf("BinOp id={%zu} operator={%s", node->id, op_string[node->node_info.bin_op.operator.token_kind]);
            printf("}\n");
            indent++;
            // Print LHS/RHS
            dump_ast(node->node_info.bin_op.LHS, source_file, indent);
            dump_ast(node->node_info.bin_op.RHS, source_file, indent);
            indent--;
            print_indentation(indent);
            printf(")\n");

            break;
        }
        case AST_BLOCK:
        {
            printf("Block id={%d}\n", (int)node->id);
            indent++;
            for (size_t i = 0; i < node->node_info.block_info.body.count; i++) {
                dump_ast(node->node_info.block_info.body.items[i], source_file, indent);
            }
            indent--;
            print_indentation(indent);
            printf(")\n");
            break;
        }
        case AST_ASSN:
        {
            char* start_ptr = start_of_name(node->node_info.assn_stmt.name_span, source_file);

            printf("AssnStmt id={%d} type={%s} name={", (int)node->id, get_type_string(node->node_info.assn_stmt.type));
            print_file_slice(start_ptr, node->node_info.assn_stmt.name_span.length, stdout);
            printf("}\n");

            indent++;
            dump_ast(node->node_info.assn_stmt.init_expr, source_file, indent);
            indent--;
            print_indentation(indent);
            printf(")\n");
            break;
        }
        case AST_EXIT:
        {
            printf("Exit id={%d}\n", (int)node->id);
            indent++;
            dump_ast(node->node_info.exit_info.expr, source_file, indent);
            indent--;
            print_indentation(indent);
            printf(")\n");
            break;
        }
        case AST_INT_LIT: // leaf
        {
            IntLitInfo int_lit = node->node_info.int_lit;
            printf("IntLit id={%d} %ld", (int)node->id, int_lit.value);
            printf(")\n");
            break;
        }
        case AST_NAME: // leaf
        {
            printf("VarName id={%d} name={", (int)node->id);

            char* start_ptr = start_of_name(node->node_info.var_name.name_span, source_file);
            print_file_slice(start_ptr, node->node_info.var_name.name_span.length, stdout);
            printf("})\n");
            break;
        }
        default:
        {
            printf("ERROR: Couldn't find AST node.");
            break;
        }
    }
}

