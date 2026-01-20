//#include "codegen/backend/arm64-gen/emitter.h"
#include "codegen/backend/ir-verify/ir_verify.h"
//#include "codegen/backend/lir/arm64/lir.h"
#if defined(MAXC_ARENA_TESTS) && MAXC_ARENA_TESTS
#include "arena/arena_test.h"
#endif

#include <assert.h>
#include <stdalign.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "arena/arena.h"
#include "ast/lexer/lexer.h"
#include "ast/parser/parser.h"
#include "ast/parser/ast_printer.h"
#include "semantics/scope.h"
#include "semantics/walker.h"
#include "semantics/def-assn-analysis/def_assn.h"
//#include "codegen/backend/frame-layout/arm64/frame_lay.h"
#include "codegen/ir-gen/mir.h"

// C++ Backend now :)
#include "codegen/backend/backend_api.h"
#include "common.h"
#include "debug.h"


// Idea:
// Parse input file given here
// Turn file into AST
// Do all semantic passes (3 so far?)
// Convert to Max IR (MIR)
// Convert MIR to X86
//  Write output to a .m file (i guess? IDK what extension to use haha)
// Use like clang to convert the file into actual bytecode

typedef struct {
    char *input_path;
    char *output_bin_path; // For the binary (e.g., -o flag)
    int error_code;
    int debug;
    bool gen_assm_file;
} Args;

Args parse_args(int argc, char **argv) {
    Args args = {0};
    
    if (argc < 2) {
        fprintf(stderr, "Usage: %s file.m [-S] [-d] [-o output_name]\n", argv[0]);
        args.error_code = 1;
        return args;
    }

    // Simple parsing loop
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (strcmp(argv[i], "-S") == 0) {
                args.gen_assm_file = true;
            } else if (strcmp(argv[i], "-d") == 0) {
                args.debug = 1;
            } else if (strcmp(argv[i], "-o") == 0) {
                if (i + 1 < argc) {
                    args.output_bin_path = argv[++i];
                } else {
                    fprintf(stderr, "Error: -o requires an argument\n");
                    args.error_code = 1; 
                    return args;
                }
            }
        } else {
            if (args.input_path == NULL) {
                args.input_path = argv[i];
            }
        }
    }
    
    if (!args.input_path) {
        fprintf(stderr, "Error: No input file provided.\n");
        args.error_code = 1;
    }
    
    return args;
}

FILE* create_assm_file(const char* name) {
    FILE* assm = fopen(name, "w");
    return assm;
}


int main(int argc, char **argv) {

    /*------------ DEBUGGING ARENA ------------ */
    #if defined(MAXC_ARENA_TESTS) && MAXC_ARENA_TESTS
        Arena a = {0};
        arena_init(&a, 256);

        test_arena_all(&a);

        arena_destroy(&a);
    #endif
    /*------------ DEBUGGING ARENA ------------ */

    // Parse input arguments
    Args args = parse_args(argc, argv);
    if (args.error_code != 0) {
        fprintf(stderr, "Error parsing input arguments.");
        return 1;
    }

    // Open file
    FILE *fp = fopen(args.input_path, "r");
    if (!fp) {
        fprintf(stderr, "Cannot open %s: %s\n", argv[1], strerror(errno));
        return 1;
    }
    
    // Read file into buffer.
    // Result just stores error code/messages
    Source source_file;
    // TODO: Check result of filling buffer for errors
    Result buffer_result  = read_source_file(fp, &source_file);
    if (buffer_result.error_code != 0) {
        fprintf(stderr, "Error filling buffer: %s", buffer_result.error_message);
        return 1;
    }
    fclose(fp);

    if (args.debug) {
        printf("%s", source_file.buffer);
        printf("\n\nBuffer size: %d\n", (int)source_file.length);
        printf("--------- LEXER ---------\n");
    }

    // Allocate big memory arena for persistent memory (until compilation finishes)
    Arena arena = {0};
    if (!arena_init(&arena, DEFAULT_BLOCK_SIZE)) {
        fprintf(stderr, "Failed to initialize arena in main.");
        return 2;
    }
    Diagnostics diags;
    diags_init(&diags, &arena, 16);
    assert(diags.items != NULL);

    
    // Lex the input buffer into tokens
    // TODO: Check result of filling buffer for errors
    int start_size = START_BUFFER_SIZE;
    TokenBuffer tokens;
    tokens.data = malloc(sizeof(Token) * start_size);
    tokens.count = 0;
    tokens.capacity = start_size;

    Result lex_result = lex_input(&tokens, &source_file);
    if (lex_result.error_code != 0) {
        fprintf(stderr, "Error lexing arguments: %s", lex_result.error_message);
    }
    if (!last_token_is_EOF(&tokens)) {
        fprintf(stderr, "Lexing error: last token was not end of file.");
        return 1;
    }

    if (args.debug) {
        print_all_tokens(&tokens, source_file.buffer);
        printf("\n");
        pretty_print_tokens(&tokens, source_file.buffer);
        printf("---------------------\n\n");
    }


    // Create AST based on token buffer

    Parser parser;
    if (!initialize_parser(&parser, &arena, &tokens, &diags)) {
        fprintf(stderr, "Failed to initialize parser.");
        return 2;
    }

    // Parse tokens and construct AST.
    ASTNode* ast_root = build_ast(&parser, &source_file);
    if (print_parser_err_msgs(&parser)) {
        printf("Compilation failed with %zu errors.", parser.error_list_size);
        return 3;
    }

    // TODO: Remove old parser errs, switch fully to diags. Some uses it some don't.
    if (print_errors(&diags, "AST BUILDER:\n")) return 3;
    if (args.debug) {
        printf("------------- AST -------------\n");
        dump_ast(ast_root, &source_file, 0);
    }
    /*------ Semantic passes ------*/

    // Scope resolver
    Resolver resolver;
    // Resolver can reference arena via 'diags'
    resolver_init(&resolver, &arena, &diags, &source_file, args.debug);
    run_resolver(ast_root, &resolver);

    if (print_errors(&diags, "RESOLVER:\n")) return 4;

    // todo: probably remove in favor of CFG-based DA :((
    DefAssn defassn;
    definite_assignment_init(&defassn, &diags, &arena, &source_file, args.debug, resolver.curr_id);
    run_definite_assignment(ast_root, &defassn);

    if (print_errors(&diags, "DEFINITE_ASSN:\n")) return 5;

    // max-- IR generation
    IRBuilder builder;
    builder_init(&builder, &arena, &diags, resolver.semantics, &source_file);
    run_mir_gen(ast_root, &builder);

    // Dump MIR in human readable format
    if (args.debug) dump_mir(&builder, stdout);

    /*------ BACKEND PASSES ------*/
    // Initialize new Diagnostics for iR
    Diagnostics ir_diags;
    diags_init(&ir_diags, &arena, 16);
    assert(ir_diags.items != NULL);

    // Initialize IR Module (useful tables)
    IRModule mod;
    ir_module_init(&mod, &builder.funcs, &resolver.semantics->name_resolution);

    // Initialize IR Verification pass
    IRVerify verifier = {0};
    ir_verifier_init(&verifier, &ir_diags, &arena, &mod);

    run_ir_verifier(&verifier, &builder.funcs);

    if (args.debug) {
        if (print_errors(&ir_diags, "IR VERIFICATION:\n")) return 6;
        fprintf(stdout, "\nIR Verifier: IR Verified!\nInstructions visited: %zu\n\n", verifier.inst_visited);
    }

    // Ensure the output folder exists
    const char* output_dir = "exe";
    ensure_directory_exists(output_dir);

    // Determine base name (e.g., "calc" from "tests/math/calc.m")
    char* base_name = get_basename_no_ext(args.input_path);
    
    // Construct the Assembly Path: "exe/calc.s"
    // We put the .s file here too so it doesn't clutter the source folder
    char asm_path[1024];
    snprintf(asm_path, sizeof(asm_path), "%s/%s.s", output_dir, base_name);

    // Run Backend with this specific path
    int backend_res = run_backend_pipeline(&builder.funcs, &mod, &arena, &ir_diags, &source_file, args.debug, asm_path);
    
    if (backend_res != 0) {
        free(base_name);
        return backend_res;
    }

    // Assemble/Link (if -S was NOT passed)
    if (!args.gen_assm_file) {
        // Construct the Binary Path: "exe/calc" (or "exe/calc.exe" on Windows)
        char bin_path[1024];
        
        // If user provided -o, use that exactly. Otherwise use our auto-generated path.
        if (args.output_bin_path) {
            strncpy(bin_path, args.output_bin_path, sizeof(bin_path));
        } else {
            snprintf(bin_path, sizeof(bin_path), "%s/%s", output_dir, base_name);
        }

        // Construct the command: clang exe/calc.s -o exe/calc
        char cmd[2048];
        snprintf(cmd, sizeof(cmd), "clang %s -o %s", asm_path, bin_path);
        
        if (args.debug) printf("[Linker Cmd]: %s\n", cmd);

        int link_res = system(cmd);
        
        if (link_res != 0) {
            fprintf(stderr, "Error: Linking failed.\n");
        } else {
            // Cleanup: Delete the .s file to keep "exe/" clean, 
            // unless you want to keep it for debugging.
            if (!args.debug) remove(asm_path);
            
            if (args.debug) printf("Successfully created: %s\n", bin_path);
        }
    } else {
        printf("Assembly generated at: %s\n", asm_path);
    }

    // Free all memory
    free(tokens.data);
    free(base_name);
    free_source(&source_file);
    free_ast_arena(&parser); // todo: just free arena; confusing naming.
    
    return 0;
}