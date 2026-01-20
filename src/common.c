#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h> // For mkdir
#include <sys/types.h>
#include "ast/parser/ast.h"
#include "ast/parser/parser.h"
#include "common.h"
#include "table/ptrtable.h"

Result read_source_file(FILE* fp, Source* out) {
    // TODO: FREE THE BUFFER AFTER PARSING!
    Result result;
    result.error_code = 0;
    // Seek to end, get location of pointer, tells buffer size
    if (fseek(fp, 0L, SEEK_END) == 0) {
        // Get size of file
        long bufsize = ftell(fp);
        if (bufsize == -1) { /* Error ? */}
        out->length = bufsize;

        // Allocate a buffer to specified size
        out->buffer = malloc(sizeof(char) * (bufsize + 1));

        // Go back to start
        if (fseek(fp, 0L, SEEK_SET) != 0) { /* Error ? */}

        // Read entire file into memory
        size_t new_len = fread(out->buffer, sizeof(char), bufsize, fp);
        if (ferror(fp) != 0) {
            fputs("Error reading file", stderr);
            result.error_message = "Error reading file";
            result.error_code = 1;
        } else {
            out->buffer[new_len++] = '\0';
        }
    }

    return result;
}

void free_source(Source* source) {
    free(source->buffer);
}

// Computes line and column based on current start location.
LineCol get_line_col_from_span(size_t start_loc, Source* source_file) {
    assert(start_loc <= source_file->length);
    LineCol line_col;
    line_col.line = 1;
    line_col.col = 1;

    size_t index = 0;
    while (index < start_loc) {
        char current_char = source_file->buffer[index];
        if (current_char == '\n') {
            line_col.line++;
            line_col.col = 1;
        }
        else {
            line_col.col++;    
        }
        index++;
    }

    return line_col;
}

// Grabs actual string name (start pointer) from span in buffer.
char* start_of_name(SrcSpan span, Source* source_file) {
    return &source_file->buffer[span.start];
}

// Prints a slice of the input file (typically a var)
void print_file_slice(char* start_ptr, size_t length, FILE* output) {
    for (size_t i = 0; i < length; i++) {
        fprintf(output, "%c", *(start_ptr + i));
    }
}

// Initialize IR Module.
void ir_module_init(IRModule* mod, Vector* funcs, PtrTable* name_res) {
    mod->funcs = funcs;
    mod->name_resolution = name_res; // found in struct Semantics
    // TODO: Add 2 other tables once they're actually used.
}

// Helper to replace extension (e.g., "test.m" -> "test.s")
char* change_extension(const char* filename, const char* new_ext) {
    const char* dot = strrchr(filename, '.');
    if (!dot || dot == filename) return NULL; // No extension found

    size_t base_len = dot - filename;
    size_t ext_len = strlen(new_ext);
    
    char* new_filename = (char*)malloc(base_len + ext_len + 1);
    memcpy(new_filename, filename, base_len);
    strcpy(new_filename + base_len, new_ext);
    
    return new_filename;
}

// Helper to strip extension entirely (e.g., "test.m" -> "test") for the binary name
char* strip_extension(const char* filename) {
    const char* dot = strrchr(filename, '.');
    if (!dot || dot == filename) return strdup(filename);

    size_t base_len = dot - filename;
    char* new_filename = (char*)malloc(base_len + 1);
    memcpy(new_filename, filename, base_len);
    new_filename[base_len] = '\0';
    
    return new_filename;
}

// Helper: Ensure the output directory exists
void ensure_directory_exists(const char* dir_path) {
    struct stat st = {0};
    if (stat(dir_path, &st) == -1) {
#ifdef _WIN32
        _mkdir(dir_path);
#else
        mkdir(dir_path, 0700);
#endif
    }
}

// Helper: Extract filename without path or extension
// Input: "tests/math/calc.m" -> Returns: "calc"
char* get_basename_no_ext(const char* path) {
    const char *slash = strrchr(path, '/');
    const char *backslash = strrchr(path, '\\'); // Handle Windows paths
    
    // Pick the last separator found
    const char *filename_start = path;
    if (slash && backslash) {
        filename_start = (slash > backslash) ? slash + 1 : backslash + 1;
    } else if (slash) {
        filename_start = slash + 1;
    } else if (backslash) {
        filename_start = backslash + 1;
    }

    // Now find the extension dot
    const char *dot = strrchr(filename_start, '.');
    
    size_t len;
    if (dot) {
        len = dot - filename_start;
    } else {
        len = strlen(filename_start);
    }

    char *result = (char*)malloc(len + 1);
    memcpy(result, filename_start, len);
    result[len] = '\0';
    return result;
}