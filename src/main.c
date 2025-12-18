#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

    // Idea:
    // Parse input file given here
    // Turn file into AST
    // Do all semantic passes (3 so far?)
    // Convert to Max IR (MIR)
    // Convert MIR to X86
    //  Write output to a .m file (i guess? IDK what extension to use haha)
    // Use like clang to convert the file into actual bytecode


int check_input(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s file.m\n", argv[0]);
        return 1;
    }

    FILE *fp = fopen(argv[1], "r");
    if (!fp) {
        fprintf(stderr, "Cannot open %s: %s\n", argv[1], strerror(errno));
        return 1;
    }

    fclose(fp);

    return 0;
}

int main(int argc, char **argv) {

    int parsed = check_input(argc, argv);
    if (parsed > 0) {
        return parsed;
    }

    
    
    return 0;



}

