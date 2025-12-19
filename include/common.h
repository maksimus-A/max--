#pragma once
#include <stdio.h>
struct Result {
    int error_code;
    const char* error_message;
};
typedef struct Result Result;

struct Source { // Source file
    const char* path;
    size_t length;
    char* buffer;
};
typedef struct Source Source;

Result read_source_file(FILE* fp, Source* out); // Should return buffer of file ??
void free_source(Source* s);