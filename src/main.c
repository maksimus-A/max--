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

typedef struct Args Args;
struct Args {
    char* input_path;
    char* output_assm_path;
    short error_code;
    short debug;
    bool gen_assm_file;
};

Args parse_args(int argc, char **argv) {
    Args args;
    args.input_path = NULL;
    args.output_assm_path = NULL;
    args.error_code = 0;
    args.debug = 0;
    args.gen_assm_file = false;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s file.m\n [-debug]", argv[0]);
        args.error_code = 1;
        return args;
    }
    args.input_path = argv[1];
    for (int i = 1; i < argc; i++) {
        if (argv[i] != NULL && argv[i][0] == '-') {
            if (argv[i][1] == 'd') {
                args.debug = 1;
            }
            /* handle other flags as needed */
            if (argv[i][1] == 'S') { // gen assembly flag
                args.gen_assm_file = true;
            }
        }
    }
    // TODO: Ensure valid input path, check for output path, etc
    args.error_code = 0;
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

    if (args.debug) {
        printf("------------- AST -------------\n");
        dump_ast(ast_root, &source_file, 0);
    }
    // TODO: Remove old parser errs, switch fully to diags. Some uses it some don't.
    if (print_errors(&diags, "AST BUILDER:\n")) return 3;

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

    // Transfers all backend work to C++.
    run_backend_pipeline(&builder.funcs, &mod, &arena, &ir_diags, &source_file, args.debug);

    // Free all memory
    free(tokens.data);
    free_source(&source_file);
    free_ast_arena(&parser); // todo: just free arena; confusing naming.
    
    return 0;
}


    /*
    // Frame layout pass
    FrameLayout frame_layout = {0};
    frame_layout_init(&frame_layout, &ir_diags, &arena, &mod);
    run_frame_layout(&frame_layout);

    if (args.debug) print_frames(&frame_layout);

    // Insert frames created into IRModule
    mod.frames = frame_layout.frames;

    // Lower IR generation pass
    LIRBuilder lir_builder = {0};
    lir_builder_init(&lir_builder, &arena, &ir_diags, &mod, &source_file);
    run_lir_builder(&lir_builder);

    if (args.debug) dump_lir(&lir_builder, stdout);
    fprintf(stdout, "\n\n\n");

    // Final pass. Generate ARM64 only (for now at least).
    ARMEmitter emitter = {0};
    // Debugging; should just find the name based on input name.
    const char* name = "mx_out.s";
    FILE* assm = create_assm_file(name);
    if (assm == NULL) {
        fprintf(stderr, "Failed to create assembly file.");
        return 6;
    }
    arm_emitter_init(&emitter, assm, &lir_builder.lir_funcs, &mod, &ir_diags);
    run_arm_emitter(&emitter);
    */