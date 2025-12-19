#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "ast/lexer/lexer.h"
#include "common.h"


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
    char* output_path;
    int error_code;
};

Args parse_args(int argc, char **argv) {
    Args args;
    if (argc < 2) {
        fprintf(stderr, "Usage: %s file.m\n", argv[0]);
        args.error_code = 1;
        return args;
    }
    args.input_path = argv[1];
    // TODO: Ensure valid input path, check for output path, etc
    args.error_code = 0;
    return args;
}


int main(int argc, char **argv) {
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
    }
    
    // Read file into buffer.
    // Result just stores error code/messages
    Source source_file;
    // TODO: Check result of filling buffer for errors
    Result buffer_result  = read_source_file(fp, &source_file);
    fclose(fp);

    // TODO: Just debug, remove (or add debug flag?)
    //printf("%s", source_file.buffer);
    //printf("Buffer size: %zu", source_file.length);

    // Lex the input buffer into tokens
    // TODO: Check result of filling buffer for errors
    int start_size = START_BUFFER_SIZE;
    TokenBuffer tokens;
    tokens.data = malloc(sizeof(char) * start_size);

    Result tokenize_result = lex_input(&tokens, &source_file);
    
    free(tokens.data);
    free_source(&source_file);
    
    return 0;

}

