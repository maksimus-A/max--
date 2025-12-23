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

Result read_source_file(FILE* fp, Source* out); // Should return buffer of file ??
void free_source(Source* s);

LineCol get_line_col_from_span(size_t start_loc, Source* source_file);