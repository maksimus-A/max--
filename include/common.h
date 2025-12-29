#pragma once
#include <stdio.h>

#define START_BUFFER_SIZE 16

// TODO: Result should also return
// a pointer to a struct I need? Right now
// it's kinda useless, could just return int.
struct Result {
    int error_code;
    const char* error_message;
    void* data; // TODO:CHECK if necessary?
};
typedef struct Result Result;

struct Source { // Source file
    const char* path;
    size_t length;
    char* buffer;
};
typedef struct Source Source;

typedef struct LineCol {
    size_t line;
    size_t col;
} LineCol;

typedef struct SrcSpan {
    // Used to store variable name + location.
    size_t length;
    size_t start;
} SrcSpan;

Result read_source_file(FILE* fp, Source* out); // Should return buffer of file ??
void free_source(Source* s);

LineCol get_line_col_from_span(size_t start_loc, Source* source_file);

// Grabs actual string name (start pointer) from span in buffer.
char* start_of_name(SrcSpan span, Source* source_file);

// Prints a slice of the input file (typically a var)
void print_file_slice(char* start_ptr, size_t length, FILE* output);