#pragma once
#include "table/ptrtable.h"
#include "vector/vec.h"
#include <stdio.h>

#define START_BUFFER_SIZE 16

// todo**: check if this works. IDK
typedef struct FrameInfo FrameInfo;

// TODO: Result should also return
// a pointer to a struct I need? Right now
// it's kinda useless, could just return int.
typedef struct Result {
    int error_code;
    const char* error_message;
    void* data; // TODO:CHECK if necessary?
}Result;

// Source file
typedef struct Source { 
    const char* path;
    size_t length;
    char* buffer;
} Source;

// Stores line and column of a symbol.
typedef struct LineCol {
    size_t line;
    size_t col;
} LineCol;

// Used to store variable name + location.
typedef struct SrcSpan {
    size_t length;
    size_t start;
} SrcSpan;

// Backend structs (IR -> ARM)

// Stores a bunch of useful tables used during
// backend analysis
typedef struct IRModule{
    // todo: initialize all ptr tables/IRModule
    // funcs == MIR functions.
    Vector* funcs;
    PtrTable* slot_type; // slot_id -> symbol type
    PtrTable* name_resolution; // slot_id -> symbol // todo: is resolved??
    // todo: below is uninitialized.
    PtrTable* VRegInfo; // vregid-> vreg size,type // todo: add in LIR_gen
    PtrTable* temp_inst; // tempid -> instruction
    // Now uninit b/c removed from backend pipeline.
    // FrameInfo* frames; // filled after frame_info pass.
} IRModule;

Result read_source_file(FILE* fp, Source* out); // Should return buffer of file ??
void free_source(Source* s);

LineCol get_line_col_from_span(size_t start_loc, Source* source_file);

// Grabs actual string name (start pointer) from span in buffer.
char* start_of_name(SrcSpan span, Source* source_file);

// Prints a slice of the input file (typically a var)
void print_file_slice(char* start_ptr, size_t length, FILE* output);

void ir_module_init(IRModule* mod, Vector* funcs, PtrTable* name_res);