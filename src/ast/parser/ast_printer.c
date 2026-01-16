#include <stdio.h>
#include <stdlib.h>
#include "ast/lexer/lexer.h"
#include "ast/parser/ast.h"
#include "common.h"

char* built_in_type_string[TYPE_TOTAL_COUNT] = {
    /*TYPE_INT*/         "int", // Converts to i32

    // Signed integers
    /*TYPE_SI64*/        "i64",

    /*TYPE_BOOL*/        "bool",
    /*TYPE_CHAR*/        "char",
    /*TYPE VOID*/        "void",
    /*TYPE UNKNOWN*/     "UNKNOWN_TYPE"
};

const char* op_string[TOK_COUNT] = {
    [PLUS] = "+",
    [MINUS] = "-",
    [MULT] = "*",
    [DIV] = "/",
    [EQQ] = "==",
    [NEQ] = "!=",
    [LESS_THAN] = "<",
    [GREATER_THAN] = ">"
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

void print_param(ParamDeclInfo param, FILE* out, Source* source_file) {
    fprintf(out, "(type={%s} name={", get_type_string(param.type));
    char* param_start = start_of_name(param.name, source_file);
    print_file_slice(param_start, param.name.length, out);
    fprintf(out, "})");
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
        case AST_FN_DEC: 
        {
            const FnDeclInfo fn = node->node_info.fn_dec;
            char* fn_start = start_of_name(fn.fn_name, source_file);

            printf("FnDecl id={%d} name={", (int)node->id);
            print_file_slice(fn_start, fn.fn_name.length, stdout);
            printf("} ret_type={%s} ", get_type_string(fn.ret_type));

            printf("params={");
            for (int i = 0; i < fn.params.count; i++) {
                print_param(VEC_AT_T(&fn.params, ParamDeclInfo, i), stdout, source_file);
                printf(" ");
            }
            ASTNode* block = fn.fn_block;
            if (block != NULL) {
                printf("\n");
                indent++;
                dump_ast(block, source_file, indent);
                indent--;
                print_indentation(indent);
            }
            printf(")\n");
            break;
        }
        case AST_FN_CALL:
        {
            // Print callee (ast_name)
            CallExprInfo call = node->node_info.fn_call;
            printf("CallExpr id={%zu} fn_name=", node->id);
            ASTNode* callee = call.callee;
            if (callee != NULL) {
                printf("\n");
                indent++;
                dump_ast(callee, source_file, indent);
                indent--;
                print_indentation(indent);
            }
            printf("(args=\n");

            int arg_indent = indent + 1;
            for (int i = 0; i < call.args.count; i++) {
                dump_ast(VEC_AT_T(&call.args, ASTNode*, i), source_file, arg_indent);
            }
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
            printf("BinOp id={%zu} operator={%s", node->id, op_string[node->node_info.bin_op.op.token_kind]);
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
        case AST_CMP_OP:
        {
            // identical to above except it prints cmpop lol.
            printf("CmpOp id={%zu} operator={%s", node->id, op_string[node->node_info.bin_op.op.token_kind]);
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
        case AST_IF:
        {
            printf("IfStmt id={%d}\n", (int)node->id);
            indent++;
            dump_ast(node->node_info.if_stmt.cond, source_file, indent);
            dump_ast(node->node_info.if_stmt.then_block, source_file, indent);
            if (node->node_info.if_stmt.else_block != NULL) {
                dump_ast(node->node_info.if_stmt.else_block, source_file, indent);
            }
            indent--;
            print_indentation(indent);
            printf(")\n");
            break;
        }
        case AST_WHILE:
        {
            printf("WhileStmt id={%d}\n", (int)node->id);
            indent++;
            dump_ast(node->node_info.while_stmt.cond, source_file, indent);
            dump_ast(node->node_info.while_stmt.loop_block, source_file, indent);
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

