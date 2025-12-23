#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "common.h"

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
