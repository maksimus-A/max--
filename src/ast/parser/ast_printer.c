#include <stdio.h>
#include <stdlib.h>
#include "ast/parser/ast.h"
#include "common.h"

char* built_in_type_string[TYPE_TOTAL_COUNT] = {
    /*TYPE_INT*/         "int",
    /*TYPE_BOOL*/        "bool",
    /*TYPE_CHAR*/        "char",
};

char* get_type_string(BuiltInType type) {
    return built_in_type_string[type];
}

// Grabs actual string name from span in buffer.
char* start_of_name(SrcSpan span, Source* source_file) {
    return &source_file->buffer[span.start];
}

// Prints a slice of the input file.
void print_file_slice(char* start_ptr, size_t length) {
    for (size_t i = 0; i < length; i++) {
        printf("%c", *(start_ptr + i));
    }
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
    (Int_Lit 123)))*/
void dump_ast(ASTNode* node, Source* source_file, int indent) {
    print_indentation(indent);
    printf("(");
    switch (node->ast_kind) {
        case AST_PROGRAM:
        {
            printf("Program\n");
            indent++;
            for (size_t i = 0; i < node->node_info.program.count; i++) {
                dump_ast(node->node_info.program.items[i], source_file, indent);
            }
            indent--;
            print_indentation(indent);
            printf(")\n");
            break;
        }
        case AST_VAR_DEC: 
        {
            VarDeclInfo var_decl = node->node_info.var_decl;
            SrcSpan name_span = var_decl.name_span;
            char* start_ptr = start_of_name(name_span, source_file);

            printf("VarDecl type={%s} name=", get_type_string(var_decl.type));
            print_file_slice(start_ptr, name_span.length);
            printf("\n");

            indent++;
            dump_ast(var_decl.init_expr, source_file, indent);
            indent--;
            print_indentation(indent);
            printf(")\n");
            break;
        }
        case AST_INT_LIT: // leaf
        {
            IntLitInfo int_lit = node->node_info.int_lit;
            printf("IntLit %ld", int_lit.value);
            printf(")\n");
            break;
        }
        default:
            break;
    }
}

